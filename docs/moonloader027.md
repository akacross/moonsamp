# MoonLoader v.027 functions

Functions MoonLoader v.027.0-preview3 adds over v.026.5-beta. They are
MoonLoader's own: they work with or without moonsamp, and they are nil on
026.5, so check before calling:

```lua
if getAllPickups then
    for _, h in ipairs(getAllPickups()) do
        print(h, getPickupModel(h), getPickupType(h))
    end
end
```

## Pickups

A pickup handle is the one the game's pickup opcodes use (`createPickup`,
`removePickup`, `hasPickupBeenCollected`): the slot in the game's pickup
array (`CPickups::aPickUps`, 620 slots of 0x20 bytes) in the low 16 bits and
the slot's reference count in the high 16. SA-MP makes its server pickups as
game pickups, so they are listed too.

| Function | Returns | Notes |
| --- | --- | --- |
| `getAllPickups()` | table | handles of every slot whose type is not 0 (`PICKUP_NONE`), as an array |
| `getPickupModel(handle)` | int | model id; 0 for a stale or bad handle |
| `getPickupType(handle)` | int | `ePickupType`, below; 0 for a stale or bad handle, and for a removed pickup |
| `getPickupPointer(handle)` | int | the `CPickup` address; 0 for a stale or bad handle |
| `getPickupPointerHandle(pointer)` | int | the handle of the `CPickup` at `pointer`; -1 when it is not a slot of the array or the slot is empty |

A handle is checked with `CPickups::GetActualPickupIndex` (0x4552A0), so one
whose slot was reused is stale. Removing a pickup does not make its handle
stale: the slot's reference count changes only when the game reuses it, so
until then the old handle still gives the slot, with type 0 and the model and
pointer it had. Check the type to tell a removed pickup.

`CPickup` (0x20 bytes), for reading it through `getPickupPointer`:

| Offset | Type | Field |
| --- | --- | --- |
| 0x00 | float | revenue value |
| 0x04 | `CObject*` | the pickup's object, when it has one |
| 0x08 | uint32 | ammo |
| 0x0C | uint32 | regeneration time |
| 0x10 | 3 x int16 | position x 8 |
| 0x16 | uint16 | money per day |
| 0x18 | int16 | model id |
| 0x1A | int16 | reference count (the handle's high 16 bits) |
| 0x1C | uint8 | type |
| 0x1D | uint8 | flags: 1 disabled, 2 empty, 4 help shown, 8 visible |

`ePickupType`:

| | | | |
| --- | --- | --- | --- |
| 0 none | 1 in shop | 2 on street | 3 once |
| 4 once, timeout | 5 once, slow timeout | 6 collectable | 7 in shop, out of stock |
| 8 money | 9 mine, inactive | 10 mine, armed | 11 sea mine, inactive |
| 12 sea mine, armed | 13 floating package | 14 floating package, floating | 15 on street, slow |
| 16 asset revenue | 17 property, locked | 18 property, for sale | 19 money, stays |
| 20 snapshot | 21 second player | 22 once, for a mission | |

## Cursor

| Function | Returns | Notes |
| --- | --- | --- |
| `isCursorActive()` | bool | true while a script has the cursor shown with `showCursor` |
