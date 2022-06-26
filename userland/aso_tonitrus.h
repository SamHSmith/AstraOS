#ifndef __ASO_TONITRUS_H
#define __ASO_TONITRUS_H

#include "../common/types.h"
#include "aos_syscalls.h"

#define DISPOSITIO_ELEMENTORUM_RVC24 0 // RVC is short for ruber, viridis, caeruleus, aka, red, green, blue

typedef struct
{
    u8 dispositio_elementorum; // layout of the elements, aka, pixel format, pixel being short for picture element
    u64 latitudo; // width
    u64 altitudo; // height
    u64 index_relativus_ad_picturam_primam;   // relative pointer to the first picture
    u64 index_relativus_ad_picturam_secundam; // relative pointer to the second picture
} PicturaAnimata; // Animated Picture, Picture endowed with spirit

#define index_absolutus_ad_picturam_primam(index_ad_picturam_animatam) \
    ( ((u8*)index_ad_picturam_animatam) + ((PicturaAnimata*)index_ad_picturam_animatam)->index_relativus_ad_picturam_primam )

#define index_absolutus_ad_picturam_secundam(index_ad_picturam_animatam) \
    ( ((u8*)index_ad_picturam_animatam) + ((PicturaAnimata*)index_ad_picturam_animatam)->index_relativus_ad_picturam_secundam )

// refert numerus paginae
u64 magnitudo_picturae_animatae_disce(u64 latitudo, u64 altitudo)
{
    u64 index_ad_picturam_primam = ((sizeof(PicturaAnimata) + 64 - 1) / 64) * 64;
    u64 magnitudo_elementorum = 3; // 3 bytes per pixel

    u64 index_ad_picturam_secundam = ((index_ad_picturam_primam + latitudo * altitudo * magnitudo_elementorum + 64 - 1) / 64) * 64;

    u64 magnitudo_picturae = ((index_ad_picturam_secundam + latitudo * altitudo * magnitudo_elementorum + 64 - 1) / 64) * 64;
    u64 numerus_paginae = ((magnitudo_picturae + 4096 - 1) / 4096);
    return numerus_paginae;
}

// index_ad_ansam_chartae_mediae_quae_creabitur - pointer to handle of middle buffer who will be created

u64 pictura_animata_crea(u64 latitudo, u64 altitudo, u64* index_ad_ansam_chartae_mediae_quae_creabitur, void* index_ad_locum_quo_ponetur)
{
    u64 index_ad_picturam_primam = ((sizeof(PicturaAnimata) + 64 - 1) / 64) * 64;
    u64 magnitudo_elementorum = 3; // 3 bytes per pixel

    u64 index_ad_picturam_secundam = ((index_ad_picturam_primam + latitudo * altitudo * magnitudo_elementorum + 64 - 1) / 64) * 64;

    u64 magnitudo_picturae = ((index_ad_picturam_secundam + latitudo * altitudo * magnitudo_elementorum + 64 - 1) / 64) * 64;
    u64 numerus_paginae = ((magnitudo_picturae + 4096 - 1) / 4096);

    u64 ansa_chartae_mediae;
    if(!aso_charta_media_crea(numerus_paginae, &ansa_chartae_mediae))
    {
        return 0;
    }

    if(!aso_chartam_mediam_pone(ansa_chartae_mediae, index_ad_locum_quo_ponetur, 0, numerus_paginae))
    {
        aso_chartam_mediam_omitte(ansa_chartae_mediae);
        return 0;
    }
    PicturaAnimata* pictura = index_ad_locum_quo_ponetur;
    pictura->dispositio_elementorum = DISPOSITIO_ELEMENTORUM_RVC24;
    pictura->latitudo = latitudo;
    pictura->altitudo = altitudo;
    pictura->index_relativus_ad_picturam_primam = index_ad_picturam_primam;
    pictura->index_relativus_ad_picturam_secundam = index_ad_picturam_secundam;

    *index_ad_ansam_chartae_mediae_quae_creabitur = ansa_chartae_mediae;
    return 1;
}

typedef struct
{
    u8 pinge_in_picturam_primam; // draw into the first frame buffer
    u8 is_there_a_cursor;
    u64 ansa_chartae_mediae_cum_pictura_animata;
    f64 draw_cursor_x;
    f64 draw_cursor_y;
} IndiciumPingendi; // Indicator of painting

#endif
