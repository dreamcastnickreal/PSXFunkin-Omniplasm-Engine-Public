#ifndef PSXF_GUARD_DISC_BUILD_H
#define PSXF_GUARD_DISC_BUILD_H

#if !defined(DISC1_ONLY) && !defined(DISC2_ONLY) && !defined(DISC3_ONLY)
#error "Define one disc build macro: DISC1_ONLY, DISC2_ONLY, or DISC3_ONLY."
#elif (defined(DISC1_ONLY) + defined(DISC2_ONLY) + defined(DISC3_ONLY)) > 1
#error "Only one DISC*_ONLY macro can be defined at a time."
#endif

#if defined(DISC1_ONLY)
#define DISC_BUILD_ID 1
#elif defined(DISC2_ONLY)
#define DISC_BUILD_ID 2
#elif defined(DISC3_ONLY)
#define DISC_BUILD_ID 3
#endif

#endif
