"""Management button that queues a new frame for the next device wake."""

from __future__ import annotations

from typing import Any

from homeassistant.components.button import ButtonEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .runtime import PhotoPainterRuntime


class RegenerateButton(CoordinatorEntity[Any], ButtonEntity):
    _attr_has_entity_name = True
    _attr_name = "Regenerate display"

    def __init__(self, coordinator: Any, runtime: PhotoPainterRuntime) -> None:
        super().__init__(coordinator)
        self.runtime = runtime
        self._attr_unique_id = f"{runtime.device_id}_regenerate"

    @property
    def device_info(self) -> dict[str, Any]:
        return {
            "identifiers": {(DOMAIN, self.runtime.device_id)},
            "name": f"PhotoPainter {self.runtime.device_id}",
            "manufacturer": "Waveshare",
            "model": "ESP32-S3 PhotoPainter",
        }

    @property
    def extra_state_attributes(self) -> dict[str, str]:
        return {"behavior": "next_connection"}

    async def async_press(self) -> None:
        await self.runtime.async_request_regenerate()


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator = hass.data[DOMAIN]["coordinators"][entry.entry_id]
    runtime: PhotoPainterRuntime = hass.data[DOMAIN]["runtimes"][entry.data["device_id"]]
    async_add_entities([RegenerateButton(coordinator, runtime)])

