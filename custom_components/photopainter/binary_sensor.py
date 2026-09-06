"""Pending update and expected-sleep status entities."""

from __future__ import annotations

from typing import Any

from homeassistant.components.binary_sensor import BinarySensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.event import async_track_time_interval
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .runtime import PhotoPainterRuntime


class _PhotoPainterBinarySensor(CoordinatorEntity[Any], BinarySensorEntity):
    _attr_has_entity_name = True

    def __init__(self, coordinator: Any, runtime: PhotoPainterRuntime, key: str, name: str) -> None:
        super().__init__(coordinator)
        self.runtime = runtime
        self.key = key
        self._attr_name = name
        self._attr_unique_id = f"{runtime.device_id}_{key}"
        self._unsub_clock = None

    async def async_added_to_hass(self) -> None:
        await super().async_added_to_hass()
        if self.key == "connection_delayed":
            self._unsub_clock = async_track_time_interval(self.hass, self._async_clock_tick, __import__("datetime").timedelta(minutes=1))

    async def async_will_remove_from_hass(self) -> None:
        if self._unsub_clock:
            self._unsub_clock()
            self._unsub_clock = None
        await super().async_will_remove_from_hass()

    async def _async_clock_tick(self, _now: Any) -> None:
        self.async_write_ha_state()

    @property
    def device_info(self) -> dict[str, Any]:
        return {
            "identifiers": {(DOMAIN, self.runtime.device_id)},
            "name": f"PhotoPainter {self.runtime.device_id}",
            "manufacturer": "Waveshare",
            "model": "ESP32-S3 PhotoPainter",
        }

    @property
    def is_on(self) -> bool:
        if self.key == "update_pending":
            return bool(self.runtime.redisplay_required)
        return self.runtime.connection_delayed()

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        return {"next_wake_at": self.runtime.next_wake_at.isoformat() if self.runtime.next_wake_at else None}


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator = hass.data[DOMAIN]["coordinators"][entry.entry_id]
    runtime: PhotoPainterRuntime = hass.data[DOMAIN]["runtimes"][entry.data["device_id"]]
    async_add_entities(
        [
            _PhotoPainterBinarySensor(coordinator, runtime, "update_pending", "Update pending"),
            _PhotoPainterBinarySensor(coordinator, runtime, "connection_delayed", "Connection delayed"),
        ]
    )
