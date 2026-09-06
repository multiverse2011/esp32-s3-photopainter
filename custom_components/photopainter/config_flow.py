"""Config and options flows for the PhotoPainter device integration."""

from __future__ import annotations

import hashlib
import secrets
from typing import Any

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.core import callback
from homeassistant.helpers import selector

from .config import normalize_entry_data
from .const import (
    DEFAULT_DAY_END,
    DEFAULT_DAY_INTERVAL,
    DEFAULT_DAY_START,
    DEFAULT_NIGHT_INTERVAL,
    DOMAIN,
    ROOMS,
)


def _entity(domain: str, device_class: str | None = None) -> selector.EntitySelector:
    if device_class is None:
        return selector.EntitySelector(selector.EntitySelectorConfig(domain=domain))
    return selector.EntitySelector(
        selector.EntitySelectorConfig(domain=domain, device_class=device_class)
    )


def _optional_entity(domain: str, device_class: str | None = None) -> Any:
    return vol.Any(None, _entity(domain, device_class))


class PhotoPainterConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Register one stable PhotoPainter device and issue its one-time key."""

    VERSION = 1
    MINOR_VERSION = 0

    def __init__(self) -> None:
        self._pending_data: dict[str, Any] | None = None
        self._pending_key: str | None = None

    async def async_step_user(self, user_input: dict[str, Any] | None = None) -> Any:
        errors: dict[str, str] = {}
        if user_input is not None:
            try:
                normalized = dict(user_input)
                device_id = normalized["device_id"]
                await self.async_set_unique_id(device_id)
                self._abort_if_unique_id_configured()
                key = secrets.token_bytes(32).hex()
                normalized["device_key_hash"] = hashlib.sha256(key.encode("utf-8")).hexdigest()
                normalized = normalize_entry_data(normalized)
            except ValueError as err:
                errors["base"] = str(err)
            else:
                self._pending_data = normalized
                self._pending_key = key
                return await self.async_step_key()

        schema = vol.Schema(
            {
                vol.Required("device_id"): str,
                vol.Optional("timezone", default="Asia/Tokyo"): str,
                vol.Optional("calendar_1", default=None): _optional_entity("calendar"),
                vol.Optional("calendar_2", default=None): _optional_entity("calendar"),
                vol.Optional("weather_entity", default=None): _optional_entity("weather"),
                **{
                    vol.Optional(f"{room}_temperature", default=None): _optional_entity("sensor", "temperature")
                    for room in ROOMS
                },
                **{
                    vol.Optional(f"{room}_humidity", default=None): _optional_entity("sensor", "humidity")
                    for room in ROOMS
                },
                vol.Optional("day_interval_minutes", default=DEFAULT_DAY_INTERVAL): vol.Coerce(int),
                vol.Optional("night_interval_minutes", default=DEFAULT_NIGHT_INTERVAL): vol.Coerce(int),
                vol.Optional("day_start", default=DEFAULT_DAY_START): str,
                vol.Optional("day_end", default=DEFAULT_DAY_END): str,
            }
        )
        return self.async_show_form(step_id="user", data_schema=schema, errors=errors)

    async def async_step_key(self, user_input: dict[str, Any] | None = None) -> Any:
        if user_input and user_input.get("confirm") is True and self._pending_data and self._pending_key:
            data = self._pending_data
            key = self._pending_key
            self._pending_data = None
            self._pending_key = None
            return self.async_create_entry(
                title=f"PhotoPainter {data['device_id']}",
                data=data,
                description_placeholders={"device_key": key},
            )
        return self.async_show_form(
            step_id="key",
            data_schema=vol.Schema({vol.Required("confirm", default=False): selector.BooleanSelector()}),
            description_placeholders={"device_key": self._pending_key or ""},
        )

    @staticmethod
    @callback
    def async_get_options_flow(config_entry: config_entries.ConfigEntry) -> config_entries.OptionsFlow:
        return PhotoPainterOptionsFlow()


class PhotoPainterOptionsFlow(config_entries.OptionsFlow):
    """Change cadence without exposing or replacing the device key."""

    def __init__(self) -> None:
        self._pending_key: str | None = None
        self._pending_hash: str | None = None

    async def async_step_init(self, user_input: dict[str, Any] | None = None) -> Any:
        if user_input is not None:
            if user_input.pop("rotate_key", False):
                key = secrets.token_bytes(32).hex()
                self._pending_key = key
                self._pending_hash = hashlib.sha256(key.encode("utf-8")).hexdigest()
                return await self.async_step_key()
            merged = dict(self.config_entry.data)
            merged.update(user_input)
            try:
                normalize_entry_data(merged)
            except ValueError:
                return self.async_show_form(
                    step_id="init",
                    data_schema=self._schema(),
                    errors={"base": "invalid_options"},
                )
            self.hass.config_entries.async_update_entry(self.config_entry, data=merged)
            return self.async_create_entry(title="", data=user_input)
        return self.async_show_form(step_id="init", data_schema=self._schema())

    async def async_step_key(self, user_input: dict[str, Any] | None = None) -> Any:
        if user_input and user_input.get("confirm") is True and self._pending_key:
            key = self._pending_key
            self._pending_key = None
            merged = dict(self.config_entry.data)
            merged["device_key_hash"] = self._pending_hash
            self._pending_hash = None
            self.hass.config_entries.async_update_entry(self.config_entry, data=merged)
            return self.async_create_entry(title="", data={})
        return self.async_show_form(
            step_id="key",
            data_schema=vol.Schema({vol.Required("confirm", default=False): selector.BooleanSelector()}),
            description_placeholders={"device_key": self._pending_key or ""},
        )

    def _schema(self) -> vol.Schema:
        data = self.config_entry.data
        return vol.Schema(
            {
                vol.Required("day_interval_minutes", default=data.get("day_interval_minutes", DEFAULT_DAY_INTERVAL)): vol.Coerce(int),
                vol.Required("night_interval_minutes", default=data.get("night_interval_minutes", DEFAULT_NIGHT_INTERVAL)): vol.Coerce(int),
                vol.Required("day_start", default=data.get("day_start", DEFAULT_DAY_START)): str,
                vol.Required("day_end", default=data.get("day_end", DEFAULT_DAY_END)): str,
                vol.Optional("rotate_key", default=False): selector.BooleanSelector(),
            }
        )
