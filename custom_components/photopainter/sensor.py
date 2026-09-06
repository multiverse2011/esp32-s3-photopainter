"""Device status sensors for PhotoPainter."""

from __future__ import annotations

from datetime import datetime, timezone
from typing import Any

from homeassistant.components.sensor import SensorDeviceClass, SensorEntity, SensorStateClass
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import PERCENTAGE, SIGNAL_STRENGTH_DECIBELS
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .runtime import PhotoPainterRuntime


def _iso(value: datetime | None) -> str | None:
    return value.astimezone(timezone.utc).isoformat() if value else None


class _PhotoPainterSensor(CoordinatorEntity[Any], SensorEntity):
    _attr_has_entity_name = True

    def __init__(self, coordinator: Any, runtime: PhotoPainterRuntime, key: str, name: str) -> None:
        super().__init__(coordinator)
        self.runtime = runtime
        self.key = key
        self._attr_name = name
        self._attr_unique_id = f"{runtime.device_id}_{key}"

    @property
    def device_info(self) -> dict[str, Any]:
        return {
            "identifiers": {(DOMAIN, self.runtime.device_id)},
            "name": f"PhotoPainter {self.runtime.device_id}",
            "manufacturer": "Waveshare",
            "model": "ESP32-S3 PhotoPainter",
        }

    @property
    def native_value(self) -> Any:
        values = {
            "last_seen": self.runtime.last_seen,
            "last_displayed": self.runtime.last_displayed,
            "battery": self.runtime.battery_percent,
            "wifi_signal": self.runtime.wifi_signal_dbm,
            "next_wake": self.runtime.next_wake_at,
            "last_error": self.runtime.last_error,
        }
        return values[self.key]

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        return {
            "pending_frame_id": self.runtime.pending_frame.frame_id if self.runtime.pending_frame else None,
            "displayed_frame_id": self.runtime.displayed_frame.frame_id if self.runtime.displayed_frame else None,
            "local_overlay": self.runtime.local_overlay,
            "firmware_version": self.runtime.firmware_version,
        }


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator = hass.data[DOMAIN]["coordinators"][entry.entry_id]
    runtime: PhotoPainterRuntime = hass.data[DOMAIN]["runtimes"][entry.data["device_id"]]
    entities = [
        _PhotoPainterSensor(coordinator, runtime, "last_seen", "Last seen"),
        _PhotoPainterSensor(coordinator, runtime, "last_displayed", "Last displayed"),
        _PhotoPainterSensor(coordinator, runtime, "battery", "Battery"),
        _PhotoPainterSensor(coordinator, runtime, "wifi_signal", "WiFi signal"),
        _PhotoPainterSensor(coordinator, runtime, "next_wake", "Next wake"),
        _PhotoPainterSensor(coordinator, runtime, "last_error", "Last error"),
    ]
    entities[2]._attr_native_unit_of_measurement = PERCENTAGE
    entities[2]._attr_device_class = SensorDeviceClass.BATTERY
    entities[2]._attr_state_class = SensorStateClass.MEASUREMENT
    entities[3]._attr_native_unit_of_measurement = SIGNAL_STRENGTH_DECIBELS
    entities[3]._attr_device_class = SensorDeviceClass.SIGNAL_STRENGTH
    entities[3]._attr_state_class = SensorStateClass.MEASUREMENT
    for entity in (entities[0], entities[1], entities[4]):
        entity._attr_device_class = SensorDeviceClass.TIMESTAMP
    async_add_entities(entities)
