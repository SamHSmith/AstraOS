

// Charta media - Middle paper, aka inter process buffers

typedef struct
{
    atomic_s64 calculus_possessorum; // counter of owners
    Kallocation buffer;
} ChartaMedia;

RWLock NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM; // kernel, for writing, for reading, lock, of array, of middle buffers
Kallocation NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM; // kernel, allocation, of array, of middle buffers
u64 NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM;

void nucleus_lineam_chartarum_mediarum_initia() // kernel, initiate the array of middle maps
{
    rwlock_create(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
    Kallocation al = {0};
    NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM = al;
    rwlock_create(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
}

// allocation - adsignatio - distribution or allotment of land - the plot of land granted - allocation

void chartam_mediam_aliena(u64 ansa_chartae)
{
    rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
    assert(ansa_chartae < NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM, "handle is within range");

    ChartaMedia* linea_chartarum = NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.memory;
    assert(atomic_s64_read(&linea_chartarum[ansa_chartae].calculus_possessorum), "handle is valid");

    Kallocation al = linea_chartarum[ansa_chartae].buffer;
    s64 prev_value = atomic_s64_decrement(&linea_chartarum[ansa_chartae].calculus_possessorum);

    if(prev_value == 1) // we are going to free
    {
        kfree_pages(al);
    }
    rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
}

// middle buffer, increase the counter of owners
// returns the kallocation of the charta you incremented reference count on.
Kallocation charta_media_calculum_possessorum_augmenta(u64 ansa_chartae)
{
    rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
    assert(ansa_chartae < NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM, "handle is within range");

    ChartaMedia* linea_chartarum = NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.memory;
    assert(atomic_s64_read(&linea_chartarum[ansa_chartae].calculus_possessorum), "handle is valid");

    Kallocation al = linea_chartarum[ansa_chartae].buffer;
    atomic_s64_increment(&linea_chartarum[ansa_chartae].calculus_possessorum);

    rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
    return al;
}

/*
 * Runs through all chartae and attempts to create
 * a new one in a slot where the reference count is zero.
 *
 * Assumes read or write lock on NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM.
 */
u64 modus_internus_charta_media_crea(u64* index_ad_ansam)
{
    ChartaMedia* linea_chartarum = NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.memory;
    for(u64 i = 0; i < NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM; i++)
    {
        s64 prev_count = atomic_s64_increment(&linea_chartarum[i].calculus_possessorum);
        assert(prev_count >= 0, "prev_count should never be less then zero, otherwise it is a cause for serious investigation");
        if(prev_count)
        {
            atomic_s64_decrement(&linea_chartarum[i].calculus_possessorum);
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
    u64 new_count = 0;
    if(NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM == 0)
    { new_count = 1; }
    else
    { new_count = NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM * 2; }

    u64 new_page_count = ((sizeof(ChartaMedia) * new_count + PAGE_SIZE - 1) / PAGE_SIZE);

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

    // add new slots
    ChartaMedia empty_slot = {0};
    for(u64 i = NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM; i < new_count; i++)
    {
        new_array[i] = empty_slot;
    }
    NUCLEUS_MAGNITUDO_LINEAE_CHARTARUM_MEDIARUM = new_count;

    return 1;
}

u64 charta_media_crea(u64 numerus_paginae, u64* index_ad_ansam)
{
    if(!numerus_paginae)
    { return 0; }

    rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);

    u64 ansa;

    if(!modus_internus_charta_media_crea(&ansa))
    {
        rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
        rwlock_acquire_write(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);

        if(!modus_internus_adsignationem_chartarum_mediarum_duplica())
        {
            rwlock_release_write(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
            return 0;
        }
        assert(modus_internus_charta_media_crea(&ansa), "after doubling array we should be able to find a valid one");

        rwlock_release_write(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
        rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
    }

    ChartaMedia* linea_chartarum = NUCLEUS_ADSIGNATIO_LINEAE_CHARTARUM_MEDIARUM.memory;
    linea_chartarum[ansa].buffer = kalloc_pages(numerus_paginae);
    if(linea_chartarum[ansa].buffer.memory == 0) // allocation failed
    {
        atomic_s64_decrement(&linea_chartarum[ansa].calculus_possessorum);
        rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
        return 0;
    }

    rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_CHARTARUM_MEDIARUM);
    *index_ad_ansam = ansa;
    return 1;
}
