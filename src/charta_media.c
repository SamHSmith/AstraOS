

// Charta media - Middle paper, aka inter process buffers

typedef struct
{
    atomic_s64 global_reference_counter;
    Kallocation buffer;
} ChartaMedia;

RWLock NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM; // kernel, for writing, for reading, lock, of array, of middle buffers
Kallocation NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM; // kernel, allocation, of array, of middle buffers
u64 NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM;

void nucleus_lineam_chartarum_mediarum_initia() // kernel, initiate the array of middle maps
{
    rwlock_create(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
    NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM = {0};
    NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM = 0;
}

// allocation - adsignatio - distribution or allotment of land - the plot of land granted - allocation

void charta_media_aliena(u64 ansa_chartae)
{
}

/*
 * Runs through all chartae and attempts to create
 * a new one in a slot where the reference count is zero.
 */
u64 modus_internus_charta_media_crea(u64* index_ad_ansam)
{
    ChartaMedia* linea_chartarum = NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.memory;
    for(u64 i = 0; i < NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM; i++)
    {
        s64 prev_count = atomic_s64_increment(&linea_chartarum[i].global_reference_counter);
        assert(prev_count >= 0, "prev_count should never be less then zero, otherwise it is a cause for serious investigation");
        if(prev_count)
        {
            atomic_s64_decrement(&linea_chartarum[i].global_reference_counter);
            continue;
        }
        *index_ad_ansam = i;
        return 1;
    }
    return 0;
}

// in future will return false if growing the array failed.
u64 modus_internus_adsignationem_chartarum_mediarum_duplica()
{
    u64 new_page_count = 0;
    if(NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.page_count == 0)
    { new_page_count = 1; }
    else
    { new_page_count = NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.page_count * 2; }

    Kallocation new_alloc = kalloc_pages(new_page_count);
    if(new_alloc.memory == 0) { return 0; }
    ChartaMedia* new_array = new_alloc.memory;
    ChartaMedia* old_array = NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.memory;
    for(u64 i = 0; i < NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM; i++)
    {
        new_array[i] = old_array[i];
    }
    kfree_pages(NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM);
    NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM = new_alloc;

    return 1;
}

u64 charta_media_crea(u64* index_ad_ansam, u64 numerus_paginae)
{
    if(!numerus_paginae)
    { return 0; }

    u64 ansa;
    while(!modus_internus_charta_media_crea(&ansa))
    {
        if(!modus_internus_adsignationem_chartarum_mediarum_duplica())
        { return 0; }
    }
    ChartaMedia* linea_chartarum = NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.memory;
    linea_chartarum[ansa].buffer = kalloc_pages(numerus_paginae);
    if(linea_chartarum[ansa].buffer.memory == 0) // allocation failed
    {
        atomic_s64_decrement(&linea_chartarum[ansa].global_reference_counter);
        return 0;
    }
    *index_ad_ansam = ansa;
    return 1;
}
