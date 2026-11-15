# cleoplus.lua

Every function in `moonloader/lib/cleoplus.lua`: CLEO+'s opcodes for MoonLoader scripts, run by moonsamp.asi with or without CLEO. `local cp = require 'cleoplus'`. Only what MoonLoader, Lua, newopcodes.lua and cleo5.lua do not already have is here; the rest is listed at the end with what to use instead.

Opcodes link to their page in [Sanny Builder's library](https://library.sannybuilder.com/#/sa); parameter names follow it. Functions with a condition return it first (`ok`), then their outputs.

## Cars

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E00](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E00) | `getCarAlarm(car)` | status | `GET_CAR_ALARM` |  |
| [0E61](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E61) | `setCarAlarm(car, status)` |  | `SET_CAR_ALARM` |  |
| [0E08](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E08) | `isCarScriptControlled(car)` | ok | `IS_CAR_SCRIPT_CONTROLLED` |  |
| [0E09](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E09) | `markCarAsNeeded(car)` |  | `MARK_CAR_AS_NEEDED` |  |
| [0E59](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E59) | `getTrailerFromCar(car)` | trailer | `GET_TRAILER_FROM_CAR` |  |
| [0E5A](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E5A) | `getCarFromTrailer(car)` | tractor | `GET_CAR_FROM_TRAILER` |  |
| [0E5F](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E5F) | `carHorn(car)` |  | `CAR_HORN` |  |
| [0E65](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E65) | `getCarCollisionIntensity(car)` | intensity | `GET_CAR_COLLISION_INTENSITY` |  |
| [0E66](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E66) | `getCarCollisionCoordinates(car)` | x, y, z | `GET_CAR_COLLISION_COORDINATES` |  |
| [0E82](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E82) | `doesCarHavePartNode(car, carNode)` | ok | `DOES_CAR_HAVE_PART_NODE` |  |
| [0E90](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E90) | `getCarCollisionSurface(car)` | surfaceType | `GET_CAR_COLLISION_SURFACE` |  |
| [0E91](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E91) | `getCarCollisionLighting(car)` | lighting | `GET_CAR_COLLISION_LIGHTING` |  |
| [0E93](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E93) | `isCarReallyInAir(car)` | ok | `IS_CAR_REALLY_IN_AIR` |  |
| [0EAD](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EAD) | `getCarProofs(car)` | bullet, fire, explosion, collision, melee | `GET_CAR_PROOFS` |  |
| [0EB4](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB4) | `setCarCoordinatesSimple(car, x, y, z)` |  | `SET_CAR_COORDINATES_SIMPLE` |  |
| [0EC9](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EC9) | `getCarRandomSeed(car)` | randomSeed | `GET_CAR_RANDOM_SEED` |  |
| [0ECD](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0ECD) | `dontDeleteCarUntilTime(car, msFromNow)` |  | `DONT_DELETE_CAR_UNTIL_TIME` |  |
| [0ECF](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0ECF) | `getTimeCarIsDead(car)` | timeIsDead | `GET_TIME_CAR_IS_DEAD` |  |
| [0EF5](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EF5) | `isCarOwnedByPlayer(car)` | ok | `IS_CAR_OWNED_BY_PLAYER` |  |
| [0EF6](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EF6) | `setCarOwnedByPlayer(car, ownedByPlayer)` |  | `SET_CAR_OWNED_BY_PLAYER` |  |
| [0EF9](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EF9) | `getCarAnimgroup(car)` | carAnimGroup | `GET_CAR_ANIMGROUP` |  |
| [0EFB](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EFB) | `isCarConvertible(car)` | ok | `IS_CAR_CONVERTIBLE` |  |
| [0EFC](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EFC) | `getCarValue(car)` | value | `GET_CAR_VALUE` |  |
| [0EFD](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EFD) | `getCarPedals(car)` | gas, brake | `GET_CAR_PEDALS` |  |
| [0EB6](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB6) | `getCarWeaponDamageLastFrame(car)` | char, weaponType, intensity | `GET_CAR_WEAPON_DAMAGE_LAST_FRAME` |  |

## Chars

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E0A](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E0A) | `isCharScriptControlled(char)` | ok | `IS_CHAR_SCRIPT_CONTROLLED` |  |
| [0E0B](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E0B) | `markCharAsNeeded(char)` |  | `MARK_CHAR_AS_NEEDED` |  |
| [0E24](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E24) | `fixCharGroundBrightnessAndFadeIn(char, fixGround, fixBrightness, fadeIn)` |  | `FIX_CHAR_GROUND_BRIGHTNESS_AND_FADE_IN` |  |
| [0E32](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E32) | `setCharCoordinatesSimple(char, x, y, z)` |  | `SET_CHAR_COORDINATES_SIMPLE` |  |
| [0E5C](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E5C) | `getCharHealthPercent(char)` | healthPercent | `GET_CHAR_HEALTH_PERCENT` |  |
| [0E83](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E83) | `getCurrentCharWeaponinfo(char)` | handle | `GET_CURRENT_CHAR_WEAPONINFO` |  |
| [0E8B](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E8B) | `getCharWeaponState(char)` | weaponState | `GET_CHAR_WEAPON_STATE` |  |
| [0E8C](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E8C) | `getCharWeaponClip(char)` | weaponClip | `GET_CHAR_WEAPON_CLIP` |  |
| [0E8E](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E8E) | `getCharCollisionSurface(char)` | surfaceType | `GET_CHAR_COLLISION_SURFACE` |  |
| [0E8F](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E8F) | `getCharCollisionLighting(char)` | lighting | `GET_CHAR_COLLISION_LIGHTING` |  |
| [0E92](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E92) | `isCharReallyInAir(char)` | ok | `IS_CHAR_REALLY_IN_AIR` |  |
| [0E96](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E96) | `clearCharPrimaryTasks(char)` |  | `CLEAR_CHAR_PRIMARY_TASKS` |  |
| [0E97](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E97) | `clearCharSecondaryTasks(char)` |  | `CLEAR_CHAR_SECONDARY_TASKS` |  |
| [0EA4](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EA4) | `isCharOnFire(char)` | ok | `IS_CHAR_ON_FIRE` |  |
| [0EA5](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EA5) | `getClosestCopNearChar(char, radius, alive, inCar, onFoot, seenInFront)` | closestCop | `GET_CLOSEST_COP_NEAR_CHAR` |  |
| [0EA6](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EA6) | `getClosestCopNearPos(x, y, z, radius, alive, inCar, onFoot)` | closestCop | `GET_CLOSEST_COP_NEAR_POS` |  |
| [0EAA](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EAA) | `setCharArrested(char)` |  | `SET_CHAR_ARRESTED` |  |
| [0EAB](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EAB) | `getCharPedstate(char)` | pedState | `GET_CHAR_PEDSTATE` |  |
| [0EAC](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EAC) | `getCharProofs(char)` | bullet, fire, explosion, collision, melee | `GET_CHAR_PROOFS` |  |
| [0EAF](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EAF) | `isCharWeaponVisibleSet(char)` | ok | `IS_CHAR_WEAPON_VISIBLE_SET` |  |
| [0EB1](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB1) | `getCharStatId(char)` | pedStat | `GET_CHAR_STAT_ID` |  |
| [0EC8](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EC8) | `getCharRandomSeed(char)` | randomSeed | `GET_CHAR_RANDOM_SEED` |  |
| [0ECB](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0ECB) | `getCharMoveState(char)` | moveState | `GET_CHAR_MOVE_STATE` |  |
| [0ECC](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0ECC) | `dontDeleteCharUntilTime(char, msFromNow)` |  | `DONT_DELETE_CHAR_UNTIL_TIME` |  |
| [0ECE](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0ECE) | `getTimeCharIsDead(char)` | timeIsDead | `GET_TIME_CHAR_IS_DEAD` |  |
| [0EFA](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EFA) | `getCharFear(char)` | fear | `GET_CHAR_FEAR` |  |
| [0E42](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E42) | `isCharDoingTaskId(char, taskId)` | ok | `IS_CHAR_DOING_TASK_ID` |  |
| [0E43](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E43) | `getCharTaskPointerById(char, taskId)` | address | `GET_CHAR_TASK_POINTER_BY_ID` |  |
| [0EFF](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EFF) | `getCharSimplestActiveTask(char)` | taskId, address | `GET_CHAR_SIMPLEST_ACTIVE_TASK` |  |
| [0E44](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E44) | `getCharKillTargetChar(char)` | killTarget | `GET_CHAR_KILL_TARGET_CHAR` |  |
| [0E46](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E46) | `isCharUsingGun(char)` | ok | `IS_CHAR_USING_GUN` |  |
| [0E47](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E47) | `isCharFighting(char)` | ok | `IS_CHAR_FIGHTING` |  |
| [0E48](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E48) | `isCharFallenOnGround(char)` | ok | `IS_CHAR_FALLEN_ON_GROUND` |  |
| [0E49](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E49) | `isCharEnteringAnyCar(char)` | ok | `IS_CHAR_ENTERING_ANY_CAR` |  |
| [0E4A](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E4A) | `isCharExitingAnyCar(char)` | ok | `IS_CHAR_EXITING_ANY_CAR` |  |
| [0E4B](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E4B) | `isCharPlayingAnyScriptAnimation(char, includeAnims)` | ok | `IS_CHAR_PLAYING_ANY_SCRIPT_ANIMATION` |  |
| [0E4C](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E4C) | `isCharDoingAnyImportantTask(char, includeAnims)` | ok | `IS_CHAR_DOING_ANY_IMPORTANT_TASK` |  |
| [0EB5](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB5) | `getCharDamageLastFrame(char)` | entity, weaponType, bodyPart, intensity | `GET_CHAR_DAMAGE_LAST_FRAME` |  |
| [0ED9](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0ED9) | `setCharIgnoreDamageAnims(char, state)` |  | `SET_CHAR_IGNORE_DAMAGE_ANIMS` |  |

## Objects and car generators

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E0C](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E0C) | `isObjectScriptControlled(object)` | ok | `IS_OBJECT_SCRIPT_CONTROLLED` |  |
| [0E0D](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E0D) | `markObjectAsNeeded(object)` |  | `MARK_OBJECT_AS_NEEDED` |  |
| [0E01](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E01) | `createObjectNoSave(modelId, x, y, z, useOffset, useGround)` | handle | `CREATE_OBJECT_NO_SAVE` |  |
| [0E71](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E71) | `getObjectCentreOfMassToBaseOfModel(object)` | distance | `GET_OBJECT_CENTRE_OF_MASS_TO_BASE_OF_MODEL` |  |
| [0E94](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E94) | `isObjectReallyInAir(object)` | ok | `IS_OBJECT_REALLY_IN_AIR` |  |
| [0E95](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E95) | `simulateObjectDamage(object, damage, weaponType)` |  | `SIMULATE_OBJECT_DAMAGE` |  |
| [0EAE](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EAE) | `getObjectProofs(object)` | bullet, fire, explosion, collision, melee | `GET_OBJECT_PROOFS` |  |
| [0ECA](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0ECA) | `getObjectRandomSeed(object)` | randomSeed | `GET_OBJECT_RANDOM_SEED` |  |
| [0E02](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E02) | `setCarGeneratorNoSave(carGenerator)` |  | `SET_CAR_GENERATOR_NO_SAVE` |  |

## The world and the game

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E04](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E04) | `getNextWeather()` | type | `GET_NEXT_WEATHER` |  |
| [0E05](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E05) | `setNextWeather(type)` |  | `SET_NEXT_WEATHER` |  |
| [0E06](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E06) | `getRainIntensity()` | intensity | `GET_RAIN_INTENSITY` |  |
| [0E07](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E07) | `setRainIntensity(intensity)` |  | `SET_RAIN_INTENSITY` |  |
| [0EB0](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB0) | `getForcedWeather()` | weather | `GET_FORCED_WEATHER` |  |
| [0E6C](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E6C) | `getDayNightBalance()` | intensity | `GET_DAY_NIGHT_BALANCE` |  |
| [0E6D](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E6D) | `getUnderwaterness()` | intensity | `GET_UNDERWATERNESS` |  |
| [0E1D](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E1D) | `isOnMission()` | ok | `IS_ON_MISSION` |  |
| [0F16](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F16) | `setOnMission(status)` |  | `SET_ON_MISSION` |  |
| [0E25](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E25) | `isOnCutscene()` | ok | `IS_ON_CUTSCENE` |  |
| [0EB7](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB7) | `isOnScriptedCutscene()` | ok | `IS_ON_SCRIPTED_CUTSCENE` |  |
| [0EB8](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB8) | `isRadarVisible()` | ok | `IS_RADAR_VISIBLE` |  |
| [0EB9](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB9) | `isHudVisible()` | ok | `IS_HUD_VISIBLE` |  |
| [0E5D](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E5D) | `isCheatActive(cheat)` | ok | `IS_CHEAT_ACTIVE` |  |
| [0EBB](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EBB) | `passTime(minutes)` |  | `PASS_TIME` |  |
| [0EC7](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EC7) | `getFadeAlpha()` | alpha | `GET_FADE_ALPHA` |  |
| [0E0F](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E0F) | `getFixedXyAspectRatio(x, y)` | x, y | `GET_FIXED_XY_ASPECT_RATIO` |  |
| [0E2C](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E2C) | `getCurrentSaveSlot()` | slot | `GET_CURRENT_SAVE_SLOT` |  |
| [0E2D](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E2D) | `isGameFirstStart()` | ok | `IS_GAME_FIRST_START` |  |

## Pickups (MoonLoader v.027 has its own: getAllPickups and the rest)

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E33](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E33) | `getPickupThisCoord(x, y, z, onlyValid)` | handle | `GET_PICKUP_THIS_COORD` |  |
| [0E34](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E34) | `getPickupModel(pickup)` | modelId | `GET_PICKUP_MODEL` |  |
| [0E38](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E38) | `getPickupPointer(pickup)` | pointer | `GET_PICKUP_POINTER` |  |
| [0E39](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E39) | `getPickupType(pickup)` | type | `GET_PICKUP_TYPE` |  |

## Input

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E3E](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E3E) | `isButtonJustPressed(pad, buttonId)` | ok | `IS_BUTTON_JUST_PRESSED` |  |
| [0E67](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E67) | `isAimButtonPressed(pad)` | ok | `IS_AIM_BUTTON_PRESSED` |  |
| [0E8D](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E8D) | `isAnyFireButtonPressed(pad)` | ok | `IS_ANY_FIRE_BUTTON_PRESSED` |  |
| [0E6E](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E6E) | `isSelectMenuJustPressed()` | ok | `IS_SELECT_MENU_JUST_PRESSED` |  |
| [0F13](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F13) | `getTimeNotTouchingPad(pad)` | timeInMs | `GET_TIME_NOT_TOUCHING_PAD` |  |
| [0E23](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E23) | `getMouseSensibility()` | sensibility | `GET_MOUSE_SENSIBILITY` |  |
| [0E60](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E60) | `setCameraControl(state)` |  | `SET_CAMERA_CONTROL` |  |
| [0E68](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E68) | `setPlayerControlPad(pad, enabled)` |  | `SET_PLAYER_CONTROL_PAD` |  |
| [0E69](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E69) | `setPlayerControlPadMovement(pad, movement)` |  | `SET_PLAYER_CONTROL_PAD_MOVEMENT` |  |

## Weapons

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E26](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E26) | `isWeaponFireType(weaponType, weaponFire)` | ok | `IS_WEAPON_FIRE_TYPE` |  |
| [0E84](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E84) | `getWeaponinfo(weaponType, weaponSkill)` | handle | `GET_WEAPONINFO` |  |
| [0E85](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E85) | `getWeaponinfoModels(weaponInfo)` | model1, model2 | `GET_WEAPONINFO_MODELS` |  |
| [0E86](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E86) | `getWeaponinfoFlags(weaponInfo)` | flags | `GET_WEAPONINFO_FLAGS` |  |
| [0E87](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E87) | `getWeaponinfoAnimgroup(weaponInfo)` | animGroup | `GET_WEAPONINFO_ANIMGROUP` |  |
| [0E88](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E88) | `getWeaponinfoTotalClip(weaponInfo)` | totalClip | `GET_WEAPONINFO_TOTAL_CLIP` |  |
| [0E89](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E89) | `getWeaponinfoFireType(weaponInfo)` | fireType | `GET_WEAPONINFO_FIRE_TYPE` |  |
| [0E8A](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E8A) | `getWeaponinfoSlot(weaponInfo)` | weaponSlot | `GET_WEAPONINFO_SLOT` |  |

## Models

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E7F](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E7F) | `getModelType(model)` | type | `GET_MODEL_TYPE` |  |
| [0EF8](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EF8) | `getModelInfo(modelId)` | modelInfo | `GET_MODEL_INFO` |  |
| [0EBA](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EBA) | `getModelPedTypeAndStat(modelId)` | pedType, pedStat | `GET_MODEL_PED_TYPE_AND_STAT` |  |
| [0E98](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E98) | `requestPriorityModel(modelId)` |  | `REQUEST_PRIORITY_MODEL` |  |
| [0E9C](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E9C) | `getModelByName(name)` | modelId | `GET_MODEL_BY_NAME` |  |
| [0E9D](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E9D) | `isModelAvailableByName(name)` | ok | `IS_MODEL_AVAILABLE_BY_NAME` |  |
| [0E9E](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E9E) | `getModelDoesntExistInRange(start, end)` | modelId | `GET_MODEL_DOESNT_EXIST_IN_RANGE` |  |
| [0E9F](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E9F) | `removeAllUnusedModels()` |  | `REMOVE_ALL_UNUSED_MODELS` |  |
| [0EA3](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EA3) | `removeModelIfUnused(modelId)` |  | `REMOVE_MODEL_IF_UNUSED` |  |
| [0F17](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F17) | `getModelNamePointer(modelId)` | address | `GET_MODEL_NAME_POINTER` |  |

## The camera

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E64](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E64) | `getCurrentCameraMode()` | mode | `GET_CURRENT_CAMERA_MODE` |  |
| [0EB2](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB2) | `getOffsetFromCameraInWorldCoords(offsetX, offsetY, offsetZ)` | x, y, z | `GET_OFFSET_FROM_CAMERA_IN_WORLD_COORDS` |  |
| [0F0E](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F0E) | `getThirdPersonCameraTarget(range, sourceX, sourceY, sourceZ)` | startX, startY, startZ, endX, endY, endZ | `GET_THIRD_PERSON_CAMERA_TARGET` |  |
| [0F0F](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F0F) | `getDistanceMultiplier()` | drawing, generating | `GET_DISTANCE_MULTIPLIER` |  |
| [0F10](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F10) | `getActiveCameraRotation()` | x, y, z | `GET_ACTIVE_CAMERA_ROTATION` |  |
| [0F11](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F11) | `getClosestWaterDistance()` | distance, closestZ | `GET_CLOSEST_WATER_DISTANCE` |  |
| [0F12](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F12) | `getCameraStruct()` | cCamera, activeCCam | `GET_CAMERA_STRUCT` |  |
| [0F14](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F14) | `getCameraRotationInputValues()` | x, y | `GET_CAMERA_ROTATION_INPUT_VALUES` |  |
| [0F15](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F15) | `setCameraRotationInputValues(x, y)` |  | `SET_CAMERA_ROTATION_INPUT_VALUES` |  |

## Entities by pointer, matrices, quats, random numbers

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0EE1](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EE1) | `getColpointCoordinates(colPoint)` | x, y, z | `GET_COLPOINT_COORDINATES` |  |
| [0EEE](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EEE) | `getEntityCoordinates(address)` | x, y, z | `GET_ENTITY_COORDINATES` |  |
| [0EEF](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EEF) | `getEntityHeading(address)` | heading | `GET_ENTITY_HEADING` |  |
| [0EB3](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EB3) | `convertDirectionToQuat(quat, x, y, z)` |  | `CONVERT_DIRECTION_TO_QUAT` |  |
| [0F0D](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0F0D) | `setMatrixLookDirection(matrix, originX, originY, originZ, dirX, dirY, dirZ)` |  | `SET_MATRIX_LOOK_DIRECTION` |  |
| [0EBC](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EBC) | `generateRandomIntInRangeWithSeed(seed, min, max)` | randomInteger | `GENERATE_RANDOM_INT_IN_RANGE_WITH_SEED` |  |
| [0EBD](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EBD) | `generateRandomFloatInRangeWithSeed(seed, min, max)` | randomFloat | `GENERATE_RANDOM_FLOAT_IN_RANGE_WITH_SEED` |  |

## Easing and noise: plain math, so computed here, not by an opcode

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0E1F](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E1F) | `ease(k, mode, way)` | result | `EASE` |  |
| [0E03](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E03) | `perlinNoise(x)` | result | `PERLIN_NOISE` |  |
| [0E29](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0E29) | `perlinNoiseFractal(x, octaves, frequency, amplitude, lacunarity, persistence)` | result | `PERLIN_NOISE_FRACTAL` |  |
| [0EF1](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EF1) | `perlinNoiseFractal2d(x, y, octaves, frequency, amplitude, lacunarity, persistence)` | result | `PERLIN_NOISE_FRACTAL_2D` |  |
| [0EF2](https://library.sannybuilder.com/#/sa/script/extensions/CLEO+/0EF2) | `perlinNoiseFractal3d(x, y, z, octaves, frequency, amplitude, lacunarity, persistence)` | result | `PERLIN_NOISE_FRACTAL_3D` |  |

## Left out: already in MoonLoader, Lua or the other libs

| Opcodes | Use instead |
| --- | --- |
| 0B20, 0B21 `READ/WRITE_CLIPBOARD_DATA` | `getClipboardText`, `setClipboardText` |
| 0D01-0D59 (36 opcodes CLEO+ shares with NewOpcodes) | the same opcodes in [newopcodes.lua](newopcodes.md) |
| 0E0E `GET_CURRENT_RESOLUTION` | `getScreenResolution` |
| 0E10, 0E11 `IS_MOUSE_WHEEL_UP/DOWN` | `getMousewheelDelta` |
| 0E12 `GET_VEHICLE_SUBCLASS` | newopcodes `getVehicleClassAndSubclass` |
| 0E13 `GET_ENTITY_TYPE` | newopcodes `getEntityTypeAndClass` |
| 0E14-0E1C extended char, car and object vars | a Lua table keyed by handle |
| 0E20 `IS_ON_SAMP` | `isSampLoaded`, `isSampAvailable` |
| 0E21, 0E22 `GET_AUDIO_SFX/RADIO_VOLUME` | newopcodes `getGameVolume` |
| 0E27 `GET_ANGLE_FROM_TWO_COORDS` | `getHeadingFromVector2d` |
| 0E28, 0E6A, 0EE2, 0EE3 struct writes, `MAKE_NOP` | the `memory` library, newopcodes `getStructField` / `setStructField` |
| 0E3B `GET_AUDIOSTREAM_INTERNAL` | the audio functions in newopcodes.lua and cleo5.lua take MoonLoader's AudioStream handles |
| 0E3D `IS_KEY_JUST_PRESSED` | `wasKeyPressed`, cleo5 `isKeyJustPressed` |
| 0E3F `CONVERT_3D_TO_SCREEN_2D` | `convert3DCoordsToScreen` |
| 0E40, 0E41 `GET_CURRENT_HOUR/MINUTE` | `getTimeOfDay` |
| 0E45 `FRAME_MOD`, 0E4D `RANDOM_PERCENT` | Lua (`math.random`) |
| 0E4E-0E58 local onscreen timers and counters | need an SCM variable; MoonLoader scripts have none |
| 0E5B `GET_CAR_DUMMY_COORD` | newopcodes `getVehicleDummyPosn` |
| 0E5E `CHANGE_PLAYER_MONEY` | `givePlayerMoney`, `getPlayerMoney` |
| 0E6B `GET_COLPOINT_LIGHTING` | newopcodes `getColDataLighting` |
| 0E6F, 0E70 custom scripts from a label, 0ED0 `RETURN_SCRIPT_EVENT`, 0F0A `RETURN_TIMES` | SCM labels; not for Lua |
| 0E72-0E7E, 0F06-0F09 lists | Lua tables |
| 0E80, 0E81, 0EC2-0EC6 string buffers | Lua strings |
| 0E99 `LOAD_ALL_PRIORITY_MODELS_NOW` | `loadAllModelsNow` |
| 0EA7-0EA9 `GET_ANY_CHAR/CAR/OBJECT_NO_SAVE_RECURSIVE` | `getAllChars`, `getAllVehicles`, `getAllObjects` |
| 0EBE, 0EE4-0EED `LOCATE_*_DISTANCE_TO_*` | `getDistanceBetweenCoords3d` |
| 0EF0 `GET_COORD_FROM_ANGLED_DISTANCE`, 0EF3 `LERP`, 0EF4, 0EF7 `CLAMP_*` | Lua math |
| 0EFE `GET_LOADED_LIBRARY` | `getModuleHandle` |

## Not yet

Coming in later versions: script events as Lua callbacks (0ED1-0EDF, 0EE0,
0F0B, 0F0C), the drawing opcodes (0E1E, 0E62, 0E63), CLEO blips (0E2A,
0E2B), render objects (0E2E-0E31, 0E35-0E37, 0E3A, 0F02-0F04), special
models and characters (0E9A, 0E9B, 0F00, 0F01, 0F05), sprite textures
(0E3C), FX particles (0EBF-0EC1) and the second player (0EA0-0EA2).

## How some differ from CLEO+

- `ease` and the noise functions are plain Lua. `ease` modes 1-10:
  quadratic, cubic, quartic, quintic, sinusoidal, exponential, circular,
  elastic, back, bounce, any other linear; ways 0 in, 1 out, 2 in-out. `ease` fixes CLEO+'s slips:
  bounce out returned 0, quintic out overshot (1.168 at 0.3) and quintic
  in-out lost its first half, elastic missed 0 and 1 at the ends. The noise
  matches CLEO+'s SimplexNoise to within float rounding.
- The AI flags (0E44, 0E46-0E4C) walk the task trees when asked; CLEO+
  reads a copy it takes once a frame. For the player with no kill task,
  `getCharKillTargetChar` gives the ped the player targets (CLEO+ meant to,
  but never reached it).
- `setCameraControl` patches the camera's 33 mouse and stick reads as CLEO+
  does, but writes 0050FB12, the operand: CLEO+ writes 0050FB10, the start
  of the fmul there.
- `isCharReallyInAir` answers by the vehicle for a ped in one: a riding ped
  has no physics of its own, so CLEO+ calls it in the air.
- Damage (0EB5, 0EB6) counts for the frame it was taken and the next, and
  the damage hooks go in the first time a script asks for it.
- `getCurrentSaveSlot`, `isGameFirstStart` and `getModelNamePointer` hook
  the game at startup, like CLEO+.
