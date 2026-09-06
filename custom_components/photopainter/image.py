"""Authenticated PNG previews for pending and successfully displayed frames."""

from __future__ import annotations

from datetime import datetime, timezone
from functools import partial
from typing import Any

from homeassistant.components.image import ImageEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .renderer import packed4_to_png, packed4_with_overlay_to_png
from .runtime import PhotoPainterRuntime


class _PhotoPainterImage(CoordinatorEntity[Any], ImageEntity):
    _attr_has_entity_name = True
    _attr_content_type = "image/png"

    def __init__(self, coordinator: Any, runtime: PhotoPainterRuntime, key: str, name: str) -> None:
        ImageEntity.__init__(self, coordinator.hass)
        CoordinatorEntity.__init__(self, coordinator)
        self.runtime = runtime
        self.key = key
        self._attr_name = name
        self._attr_unique_id = f"{runtime.device_id}_{key}"
        self._attr_image_last_updated = datetime.now(timezone.utc)

    @property
    def device_info(self) -> dict[str, Any]:
        return {
            "identifiers": {(DOMAIN, self.runtime.device_id)},
            "name": f"PhotoPainter {self.runtime.device_id}",
            "manufacturer": "Waveshare",
            "model": "ESP32-S3 PhotoPainter",
        }

    def _handle_coordinator_update(self) -> None:
        frame = self.runtime.pending_frame if self.key == "pending_frame" else self.runtime.displayed_frame
        if frame is not None:
            updated = frame.generated_at
            if self.key == "displayed_frame" and self.runtime.last_displayed:
                updated = max(updated, self.runtime.last_displayed)
            self._attr_image_last_updated = updated
        super()._handle_coordinator_update()

    async def async_image(self) -> bytes | None:
        frame = self.runtime.pending_frame if self.key == "pending_frame" else self.runtime.displayed_frame
        if frame is None:
            return None
        self._attr_image_last_updated = max(frame.generated_at, self.runtime.last_displayed or frame.generated_at) if self.key == "displayed_frame" else frame.generated_at
        if self.key == "displayed_frame" and self.runtime.local_overlay != "none":
            return await self.hass.async_add_executor_job(
                partial(
                    packed4_with_overlay_to_png,
                    frame.data,
                    overlay=self.runtime.local_overlay,
                    source_time=self.runtime.overlay_source_time,
                    timezone_name=self.runtime.entry.data.get("timezone", "Asia/Tokyo"),
                ),
            )
        return await self.hass.async_add_executor_job(packed4_to_png, frame.data)


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator = hass.data[DOMAIN]["coordinators"][entry.entry_id]
    runtime: PhotoPainterRuntime = hass.data[DOMAIN]["runtimes"][entry.data["device_id"]]
    async_add_entities(
        [
            _PhotoPainterImage(coordinator, runtime, "pending_frame", "Pending frame"),
            _PhotoPainterImage(coordinator, runtime, "displayed_frame", "Displayed frame"),
        ]
    )
