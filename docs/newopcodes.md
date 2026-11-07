# newopcodes.lua

Every function in `moonloader/lib/newopcodes.lua`: NewOpcodes (0D00-0D7F) for MoonLoader scripts, run by moonsamp.asi with or without CLEO. `local no = require 'newopcodes'`.

Opcodes link to their page in [Sanny Builder's library](https://library.sannybuilder.com/#/sa); parameter names follow it. Functions with a condition return it first (`ok`), then their outputs.

## Matrices, entities, strings and game state

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0D00](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D00) | `multiplyMatrices(a, b, out)` |  | `MULTIPLY_MATRICES` | out = a * b |
| [0D01](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D01) | `rotateMatrixOnAxis(m, x, y, z, angle, combineOp)` |  | `ROTATE_MATRIX_ON_AXIS` |  |
| [0D02](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D02) | `getMatrixXAngle(matrix)` | xAngle | `GET_MATRIX_X_ANGLE` |  |
| [0D03](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D03) | `getMatrixYAngle(matrix)` | yAngle | `GET_MATRIX_Y_ANGLE` |  |
| [0D04](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D04) | `getMatrixZAngle(matrix)` | zAngle | `GET_MATRIX_Z_ANGLE` |  |
| [0D05](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D05) | `setMatrixPosition(matrix, x, y, z)` |  | `SET_MATRIX_POSITION` |  |
| [0D06](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D06) | `getMatrixPosition(matrix)` | x, y, z | `GET_MATRIX_POSITION` |  |
| [0D07](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D07) | `getCoordsOffsetsRelativeToMatrix(x, y, z, m)` | x, y, z | `GET_COORDS_OFFSETS_RELATIVE_TO_MATRIX` |  |
| [0D08](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D08) | `setMatrixRotation(matrix, x, y, z)` |  | `SET_MATRIX_ROTATION` | radians |
| [0D09](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D09) | `copyMatrix(dst, src)` |  | `COPY_MATRIX` |  |
| [0D0A](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D0A) | `getOffsetFromMatrixInWorldCoords(matrix, x, y, z)` | x, y, z | `GET_OFFSET_FROM_MATRIX_IN_WORLD_COORDS` |  |
| [0D0B](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D0B) | `getCharBoneMatrix(char, bone)` | ok, matrix | `GET_CHAR_BONE_MATRIX` |  |
| [0D0C](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D0C) | `getCarComponentMatrix(car, componentName)` | ok, handle | `GET_CAR_COMPONENT_MATRIX` |  |
| [0D0D](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D0D) | `getCarComponent(car, name)` | ok, handle | `GET_CAR_COMPONENT` |  |
| [0D0E](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D0E) | `setCarComponentState(car, component, state)` | ok | `SET_CAR_COMPONENT_STATE` |  |
| [0D0F](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D0F) | `setCarModelAlpha(car, alpha)` | ok | `SET_CAR_MODEL_ALPHA` |  |
| [0D10](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D10) | `setCharModelAlpha(char, alpha)` | ok | `SET_CHAR_MODEL_ALPHA` |  |
| [0D11](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D11) | `setObjectModelAlpha(object, alpha)` | ok | `SET_OBJECT_MODEL_ALPHA` |  |
| [0D12](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D12) | `setCarComponentModelAlpha(car, componentName, alpha)` | ok | `SET_CAR_COMPONENT_MODEL_ALPHA` |  |
| [0D13](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D13) | `setMatrixXRotation(matrix, angle)` |  | `SET_MATRIX_X_ROTATION` |  |
| [0D14](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D14) | `setMatrixYRotation(matrix, angle)` |  | `SET_MATRIX_Y_ROTATION` |  |
| [0D15](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D15) | `setMatrixZRotation(matrix, angle)` |  | `SET_MATRIX_Z_ROTATION` |  |
| [0D16](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D16) | `setMatrixRotationFromQuat(m, q)` |  | `SET_MATRIX_ROTATION_FROM_QUAT` |  |
| [0D17](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D17) | `setQuatFromMatrix(m, q)` |  | `SET_QUAT_FROM_MATRIX` |  |
| [0D18](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D18) | `rotateQuatOnAxis(q, x, y, z, angle, combineOp)` |  | `ROTATE_QUAT_ON_AXIS` |  |
| [0D19](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D19) | `getNormalisedQuat(src, dst)` |  | `GET_NORMALISED_QUAT` |  |
| [0D1A](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D1A) | `multiplyQuats(a, b, out)` |  | `MULTIPLY_QUATS` |  |
| [0D1B](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D1B) | `getEntityTypeAndClass(pointer)` | type, vtable | `GET_ENTITY_TYPE_AND_CLASS` |  |
| [0D1C](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D1C) | `normaliseVector(vector)` |  | `NORMALISE_VECTOR` |  |
| [0D1D](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D1D) | `interpolateMatrix(out, from, to, t)` |  | `INTERPOLATE_MATRIX` |  |
| [0D1E](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D1E) | `quatSlerp(out, from, to, t)` |  | `QUAT_SLERP` |  |
| [0D1F](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D1F) | `getComponentChildComponent(component)` | child | `GET_COMPONENT_CHILD_COMPONENT` |  |
| [0D20](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D20) | `getComponentNextComponent(component)` | nextComponent | `GET_COMPONENT_NEXT_COMPONENT` |  |
| [0D21](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D21) | `getComponentName(component)` | name | `GET_COMPONENT_NAME` |  |
| [0D22](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D22) | `getComponentWorldMatrix(component)` | worldMatrix | `GET_COMPONENT_WORLD_MATRIX` |  |
| [0D23](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D23) | `getComponentModellingMatrix(component)` | modellingMatrix | `GET_COMPONENT_MODELLING_MATRIX` |  |
| [0D24](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D24) | `initialiseQuat(q, x, y, z, w)` |  | `INITIALISE_QUAT` |  |
| [0D25](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D25) | `initialiseMatrix(matrix, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)` |  | `INITIALISE_MATRIX` | right, flags, up, pad, at, pad, pos, pad |
| [0D26](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D26) | `initialiseVector(vector, x, y, z)` |  | `INITIALISE_VECTOR` |  |
| [0D27](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D27) | `memcpy(src, dst, size)` |  | `MEMCPY` |  |
| [0D28](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D28) | `getVectorElements(vector)` | x, y, z | `GET_VECTOR_ELEMENTS` |  |
| [0D29](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D29) | `getQuatElements(quat)` | x, y, z, angle | `GET_QUAT_ELEMENTS` |  |
| [0D2A](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D2A) | `getCarNumCollidedEntities(car)` | numCollidedEntities | `GET_CAR_NUM_COLLIDED_ENTITIES` |  |
| [0D2B](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D2B) | `getCharNumCollidedEntities(char)` | numCollidedEntities | `GET_CHAR_NUM_COLLIDED_ENTITIES` |  |
| [0D2C](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D2C) | `getObjectNumCollidedEntities(object)` | numCollidedEntities | `GET_OBJECT_NUM_COLLIDED_ENTITIES` |  |
| [0D2D](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D2D) | `getLocalTime()` | year, month, dayOfWeek, day, hour, minute, second, milliseconds | `GET_LOCAL_TIME` | year, month, day of week, day, h, m, s, ms |
| [0D2E](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D2E) | `setThreadVar(script pointer, var, value)` |  | `SET_THREAD_VAR` |  |
| [0D2F](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D2F) | `getThreadVar(thread, var)` | result | `GET_THREAD_VAR` |  |
| [0D30](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D30) | `getCharBone(ped, bone name or frame index)` | ok, bone | `GET_CHAR_BONE` |  |
| [0D31](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D31) | `getBoneOffset(bone)` | offsetVector | `GET_BONE_OFFSET` |  |
| [0D32](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D32) | `getBoneQuat(bone)` | quat | `GET_BONE_QUAT` |  |
| [0D33](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D33) | `setCarDoorWindowState(car, door, windowState)` |  | `SET_CAR_DOOR_WINDOW_STATE` |  |
| [0D34](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D34) | `getCarCollidedEntities(car)` | entity0, entity1, entity2, entity3, entity4, entity5 | `GET_CAR_COLLIDED_ENTITIES` |  |
| [0D35](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D35) | `getCharCollidedEntities(char)` | entity0, entity1, entity2, entity3, entity4, entity5 | `GET_CHAR_COLLIDED_ENTITIES` |  |
| [0D36](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D36) | `getObjectCollidedEntities(object)` | entity0, entity1, entity2, entity3, entity4, entity5 | `GET_OBJECT_COLLIDED_ENTITIES` |  |
| [0D37](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D37) | `setStructParam(pointer, index, value)` |  | `SET_STRUCT_PARAM` | 4-byte elements |
| [0D38](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D38) | `getStructParam(struc, param)` | value | `GET_STRUCT_PARAM` |  |
| [0D39](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D39) | `getCharMaxHealth(char)` | maxHealth | `GET_CHAR_MAX_HEALTH` |  |
| [0D3A](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D3A) | `getCollisionBetweenPoints(from xyz, to xyz, buildings, vehicles, peds, objects, dummies, seeThrough, cameraIgnore, shootThrough, ignoreEntity, colPoint)` | hit, x, y, z, entity | `GET_COLLISION_BETWEEN_POINTS` |  |
| [0D3B](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D3B) | `getColDataNormalVector(colPoint)` | x, y, z | `GET_COL_DATA_NORMAL_VECTOR` |  |
| [0D3C](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D3C) | `getColDataSurface(colpoint)` | surface | `GET_COL_DATA_SURFACE` |  |
| [0D3D](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D3D) | `getColDataLighting(colPoint)` | lighting | `GET_COL_DATA_LIGHTING` |  |
| [0D3E](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D3E) | `getColDataDepth(colPoint)` | depth | `GET_COL_DATA_DEPTH` |  |
| [0D3F](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D3F) | `findIntersectionBetweenCircles(x1, y1, r1, x2, y2, r2)` | ok, p1X, p1Y, p2X, p2Y | `FIND_INTERSECTION_BETWEEN_CIRCLES` |  |
| [0D43](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D43) | `getTxdId(txd)` | id | `GET_TXD_ID` |  |
| [0D47](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D47) | `getModelTxdId(model)` | ok, txdId | `GET_MODEL_TXD_ID` |  |
| [0D48](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D48) | `getModelCrc(model)` | ok, crc32Key | `GET_MODEL_CRC` |  |
| [0D49](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D49) | `stringCmp(strA, strB)` | ok, result | `STRING_CMP` | true when equal |
| [0D4A](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D4A) | `stringCat(buffer pointer, text)` |  | `STRING_CAT` |  |
| [0D4B](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D4B) | `stringStr(string, substring)` | ok, result | `STRING_STR` | a pointer into the first |
| [0D4C](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D4C) | `stringLen(string)` | length | `STRING_LEN` |  |
| [0D4D](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D4D) | `stringCpy(text, buffer pointer)` |  | `STRING_CPY` |  |
| [0D4E](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D4E) | `getStructField(pointer, offset, size)` | result | `GET_STRUCT_FIELD` |  |
| [0D4F](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D4F) | `setStructField(pointer, offset, size, value or text)` |  | `SET_STRUCT_FIELD` |  |
| [0D55](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D55) | `getSunColors()` | coreRed, coreBlue, coreGreen, glowRed, glowBlue, glowGreen | `GET_SUN_COLORS` | core rgb, glow rgb |
| [0D56](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D56) | `getSunScreenCoors()` | x, y | `GET_SUN_SCREEN_COORS` |  |
| [0D57](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D57) | `getSunWorldCoors()` | ok, x, y, z | `GET_SUN_WORLD_COORS` | false below the horizon |
| [0D58](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D58) | `getSunSize()` | core, glow | `GET_SUN_SIZE` | core, glow |
| [0D59](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D59) | `getCurrentWeather()` | currentWeather | `GET_CURRENT_WEATHER` |  |
| [0D5A](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D5A) | `getTrafficlightsCurrentColor()` | ns, we | `GET_TRAFFICLIGHTS_CURRENT_COLOR` | north-south, west-east |
| [0D5C](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D5C) | `getCarLightDamageStatus(car, light)` | damageState | `GET_CAR_LIGHT_DAMAGE_STATUS` |  |
| [0D5D](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D5D) | `setCarLightDamageStatus(car, light, damageState)` |  | `SET_CAR_LIGHT_DAMAGE_STATUS` |  |
| [0D5E](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D5E) | `getVehicleClassAndSubclass(vehicle)` | class, subclass | `GET_VEHICLE_CLASS_AND_SUBCLASS` |  |
| [0D5F](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D5F) | `getVehicleDummyPosn(car, dummy, world, mirrored)` | ok, x, y, z | `GET_VEHICLE_DUMMY_POSN` |  |
| [0D60](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D60) | `createProjectile(type, from entity, origin xyz, target xyz, target entity, force)` | ok | `CREATE_PROJECTILE` |  |

## Drawing

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0D40](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D40) | `drawShape(primType, texture, numVerts, verts, vertexAlpha, srcBlend, dstBlend, unused)` | ok | `DRAW_SHAPE` | false when the queue is full |
| [0D41](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D41) | `setupShapeVertex(verts, vertex 1.., x, y, z, rhw, r, g, b, a, u, v, invertX, invertY)` |  | `SETUP_SHAPE_VERTEX` |  |
| [0D45](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D45) | `rotateShapeVertices(verts, count, x, y, angle)` |  | `ROTATE_SHAPE_VERTICES` |  |
| [0D42](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D42) | `loadTxd(name, path)` | ok | `LOAD_TXD` |  |
| [0D44](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D44) | `findTextureInTxdWithName(texture, txd)` | ok, texture | `FIND_TEXTURE_IN_TXD_WITH_NAME` |  |
| [0D46](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D46) | `findTextureInTxdWithId(texture, txd slot)` | ok, texture | `FIND_TEXTURE_IN_TXD_WITH_ID` |  |
| [0D50](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D50) | `drawTemporaryShadow(type, x, y, z, width, height, angle, distance, texture, intensity, r, g, b, realTimeShadow)` |  | `DRAW_TEMPORARY_SHADOW` |  |
| [0D51](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D51) | `drawPermanentShadow(type, x, y, z, width, height, angle, distance, texture, intensity, r, g, b, ms)` |  | `DRAW_PERMANENT_SHADOW` |  |
| [0D52](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D52) | `drawTemporaryLight(type, x, y, z, dirX, dirY, dirZ, radius, r, g, b, entity)` |  | `DRAW_TEMPORARY_LIGHT` |  |
| [0D53](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D53) | `drawTemporaryCorona(texture, r, g, b, a, entity, x, y, z, size)` |  | `DRAW_TEMPORARY_CORONA` |  |
| [0D54](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D54) | `drawTemporaryCoronaEx(texture, r, g, b, a, entity, x, y, z, size, farClip, nearClip, flare, reflection, checkObstacles, flashWhileFading, fadeSpeed, onlyFromBelow)` |  | `DRAW_TEMPORARY_CORONA_EX` |  |
| [0D5B](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D5B) | `drawSpotlight(fromX, fromY, fromZ, toX, toY, toZ, baseRadius, targetRadius, shadow, intensity, flag1, flag2)` |  | `DRAW_SPOTLIGHT` |  |
| [0D61](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D61) | `loadTextureFromBmpFile(bmp, mask bmp or '')` | ok, texture | `LOAD_TEXTURE_FROM_BMP_FILE` |  |
| [0D64](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D64) | `loadTextureFromPngFile(png)` | ok, texture | `LOAD_TEXTURE_FROM_PNG_FILE` |  |
| [0D7C](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D7C) | `loadTextureFromDdsFile(dds)` | ok, texture | `LOAD_TEXTURE_FROM_DDS_FILE` |  |
| [0D7D](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D7D) | `cleanLoadedTexture(texture)` |  | `CLEAN_LOADED_TEXTURE` |  |
| [0D65](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D65) | `printTemporaryText(text, x, y, scaleX, scaleY, font)` |  | `PRINT_TEMPORARY_TEXT` |  |
| [0D66](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D66) | `printTemporaryTextEx(text, x, y, scaleX, scaleY, font, proportional, align, wrap, justify, r, g, b, a, outline, shadow, dropR, dropG, dropB, dropA, background, backR, backG, backB, backA)` |  | `PRINT_TEMPORARY_TEXT_EX` |  |
| [0D7E](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D7E) | `draw2dSprite(texture, x1, y1, x2, y2, r, g, b, a, angle)` |  | `DRAW_2D_SPRITE` |  |
| [0D7F](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D7F) | `draw2dSpriteWithGradient(texture, cornerAx, cornerAy, cornerBx, cornerBy, red0, green0, blue0, alpha0, red1, green1, blue1, alpha1, red2, green2, blue2, alpha2, red3, green3, blue3, alpha3, angle)` |  | `DRAW_2D_SPRITE_WITH_GRADIENT` |  |

## Audio streams

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| 0D67 | `setAudioStreamPositionTime(stream, seconds)` |  |  |  |
| 0D68 | `getAudioStreamPositionTime(stream)` | seconds |  |  |
| 0D69 | `setAudioStreamEffect(stream, BASS_FX type, priority)` | the effect |  |  |
| 0D6A | `removeAudioStreamEffect(stream, effect)` |  |  |  |
| 0D6B | `setAudioStreamEffectParameters(effect, pointer to its BASS struct)` |  |  |  |
| 0D6C | `getAudioStreamEffectParameters(effect, pointer to fill)` |  |  |  |
| 0D6D | `resetAudioStreamEffect(effect)` |  |  |  |
| 0D6E | `setAudioStreamAttribute(stream, BASS_ATTRIB, value)` |  |  |  |
| 0D6F | `getAudioStreamAttribute(stream, BASS_ATTRIB)` | value |  |  |
| 0D70 | `set3dAudioStreamAttributes(stream, mode, min, max, insideAngle, outsideAngle, outsideVolume)` |  |  | -1 leaves one as it is |
| 0D71 | `get3dAudioStreamAttributes(stream)` | mode, min, max, insideAngle, outsideAngle, outsideVolume |  |  |

## Components, atomics, volume and screen size

| Opcode | Function | Returns | SCM name | Notes |
| --- | --- | --- | --- | --- |
| [0D74](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D74) | `getComponentParentComponent(component)` | parentComponent | `GET_COMPONENT_PARENT_COMPONENT` |  |
| [0D75](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D75) | `getComponentNumObjects(component)` | numObjects | `GET_COMPONENT_NUM_OBJECTS` |  |
| [0D76](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D76) | `getComponentObject(component, object)` | ok, componentObject | `GET_COMPONENT_OBJECT` |  |
| [0D77](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D77) | `hideObjectAtomic(objectAtomic, hide)` |  | `HIDE_OBJECT_ATOMIC` |  |
| [0D78](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D78) | `getObjectAtomicFlag(object, atomicFlag)` | state | `GET_OBJECT_ATOMIC_FLAG` |  |
| [0D79](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D79) | `setObjectAtomicFlag(object, atomicFlag, state)` |  | `SET_OBJECT_ATOMIC_FLAG` |  |
| [0D7A](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D7A) | `getObjectAtomicNumMaterials(object)` | ok, numMaterials | `GET_OBJECT_ATOMIC_NUM_MATERIALS` |  |
| [0D7B](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D7B) | `getObjectAtomicMaterialTexture(object, material)` | ok, texture | `GET_OBJECT_ATOMIC_MATERIAL_TEXTURE` |  |
| [0D72](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D72) | `getGameVolume(asFloat)` | sfx volume, radio volume | `GET_GAME_VOLUME` |  |
| [0D73](https://library.sannybuilder.com/#/sa/script/extensions/NewOpcodes/0D73) | `getScreenWidthAndHeight(asFloat)` | width, height | `GET_SCREEN_WIDTH_AND_HEIGHT` |  |
