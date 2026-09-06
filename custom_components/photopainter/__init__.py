"""Home Assistant integration for the PhotoPainter Layout B display."""

from __future__ import annotations

from typing import TYPE_CHECKING
from typing import Any

from .config import normalize_entry_data
from .const import DOMAIN

if TYPE_CHECKING:
    from homeassistant.config_entries import ConfigEntry
    from homeassistant.core import HomeAssistant

PLATFORMS = ("sensor", "binary_sensor", "button", "image")


async def async_setup(hass: HomeAssistant, _config: dict[str, Any]) -> bool:
    from .api import register_views

    hass.data.setdefault(DOMAIN, {}).setdefault("runtimes", {})
    hass.data[DOMAIN].setdefault("coordinators", {})
    register_views(hass)
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    from .api import register_views
    from .coordinator import PhotoPainterCoordinator
    from .runtime import PhotoPainterRuntime

    # Config entries created by older development snapshots are normalized on
    # load; this only adds defaults and never creates a secret.
    normalized = normalize_entry_data(dict(entry.data))
    if normalized != dict(entry.data):
        hass.config_entries.async_update_entry(entry, data=normalized)
    runtime = PhotoPainterRuntime(hass, entry)
    await runtime.async_load()
    coordinator = PhotoPainterCoordinator(hass, entry, runtime)
    hass.data.setdefault(DOMAIN, {}).setdefault("runtimes", {})[normalized["device_id"]] = runtime
    hass.data[DOMAIN].setdefault("coordinators", {})[entry.entry_id] = coordinator
    entry.async_on_unload(entry.add_update_listener(_async_entry_updated))
    await coordinator.async_start()
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    coordinator = hass.data.get(DOMAIN, {}).get("coordinators", {}).pop(entry.entry_id, None)
    if coordinator is not None:
        await coordinator.async_stop()
    runtime = hass.data.get(DOMAIN, {}).get("runtimes", {}).pop(entry.data.get("device_id"), None)
    unloaded = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    return unloaded


async def _async_entry_updated(hass: HomeAssistant, entry: ConfigEntry) -> None:
    coordinator = hass.data.get(DOMAIN, {}).get("coordinators", {}).get(entry.entry_id)
    if coordinator is not None:
        await coordinator.async_reconfigure(dict(entry.data))
