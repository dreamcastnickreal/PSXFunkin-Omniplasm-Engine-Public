/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef PSXF_GUARD_CHARACTER_MAPPING_H
#define PSXF_GUARD_CHARACTER_MAPPING_H

#include "character.h"

// Character name-to-constructor mapping for runtime character swapping via events.
// Implemented in stage.c (which has access to all character headers).

Character* CharMap_GetCharacterByName(const char *name);
u8 CharMap_GetAnimationByName(const char *name, const char *prefix);

#endif
