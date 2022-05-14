#define PRETIUM_MAXIMUM_ABSOLUTUM_SEMAPHORI_MEDII (S64_MAX >> 1)

typedef struct
{
    atomic_s64 calculus_possessorum; // counter of owners
    atomic_s64 pretium_nunc;
    s64 pretium_maximum; // does not change over the lifetime of the semaphore
} SemaphorumMedium;

RWLock NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM; // kernel, for writing, for reading, lock, of array, of middle buffers
Kallocation NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM; // kernel, allocation, of array, of middle buffers
u64 NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM;

void nucleus_lineam_semaphororum_mediorum_initia() // kernel, initiate the array of middle maps
{
    rwlock_create(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    Kallocation al = {0};
    NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM = al;
    rwlock_create(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
}

// allocation - adsignatio - distribution or allotment of land - the plot of land granted - allocation

void semaphorum_medium_omitte(u64 ansa_semaphori)
{
    rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    assert(ansa_semaphori < NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM, "semaphore handle is within range");

    SemaphorumMedium* linea_semaphororum = NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM.memory;
    assert(atomic_s64_read(&linea_semaphororum[ansa_semaphori].calculus_possessorum), "handle is valid");

    s64 prev_value = atomic_s64_decrement(&linea_semaphororum[ansa_semaphori].calculus_possessorum);
    assert(prev_value >= 0, "We didn't double free the semaphore");

    rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
}

// middle semaphore, increase the counter of owners
void semaphorum_medium_calculum_possessorum_augmenta(u64 ansa_semaphori)
{
    rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    assert(ansa_semaphori < NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM, "handle is within range");

    SemaphorumMedium* linea_semaphororum = NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM.memory;
    s64 prev_value = atomic_s64_increment(&linea_semaphororum[ansa_semaphori].calculus_possessorum);

    assert(prev_value > 0, "handle was valid");
    rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
}

u64 semaphorum_medium_suscita(u64 ansa_semaphori, u64 numerus_suscitandi, s64* index_ad_pretium_prius)
{
    if(numerus_suscitandi >= PRETIUM_MAXIMUM_ABSOLUTUM_SEMAPHORI_MEDII)
    { return 0; }

    rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    assert(ansa_semaphori < NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM, "handle is within range");

    SemaphorumMedium* linea_semaphororum = NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM.memory;

    // increment ref count because we are operating on the semaphore and don't want it to disappear.
    s64 prev_value = atomic_s64_increment(&linea_semaphororum[ansa_semaphori].calculus_possessorum);

    assert(prev_value > 0, "handle was valid");

    {
        s64 previous_sem_value;
        if(atomic_s64_add_bounded(
            &linea_semaphororum[ansa_semaphori].pretium_nunc,
            (s64)numerus_suscitandi,
            linea_semaphororum[ansa_semaphori].pretium_maximum,
            &previous_sem_value))
        {
            // we have successfully roused the semaphore
            atomic_s64_decrement(&linea_semaphororum[ansa_semaphori].calculus_possessorum);
            rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);

            *index_ad_pretium_prius = previous_sem_value;
            return 1;
        }
    }

    atomic_s64_decrement(&linea_semaphororum[ansa_semaphori].calculus_possessorum);
    rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    return 0;
}

u64 semaphorum_medium_expectare_conare(u64 ansa_semaphori)
{
    rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    assert(ansa_semaphori < NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM, "handle is within range");

    SemaphorumMedium* linea_semaphororum = NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM.memory;
    s64 prev_value = atomic_s64_increment(&linea_semaphororum[ansa_semaphori].calculus_possessorum);

    assert(prev_value > 0, "handle was valid");
    s64 previous_sem_value;
    u64 success = atomic_s64_decrement_if_greater_than_one(&linea_semaphororum[ansa_semaphori].pretium_nunc, &previous_sem_value);

    atomic_s64_decrement(&linea_semaphororum[ansa_semaphori].calculus_possessorum);

    rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    return success;
}

/*
 * Runs through all semaphori and attempts to create
 * a new one in a slot where the reference count is zero.
 *
 * Assumes read or write lock on NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM.
 */
u64 modus_internus_semaphorum_medium_crea(u64* index_ad_ansam)
{
    SemaphorumMedium* linea_semaphororum = NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM.memory;
    for(u64 i = 0; i < NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM; i++)
    {
        s64 prev_count = atomic_s64_increment(&linea_semaphororum[i].calculus_possessorum);
        assert(prev_count >= 0, "prev_count should never be less then zero, otherwise it is a cause for serious investigation");
        if(prev_count)
        {
            atomic_s64_decrement(&linea_semaphororum[i].calculus_possessorum);
            continue;
        }
        *index_ad_ansam = i;
        return 1;
    }
    return 0;
}

// in future will return false if growing the array failed.
u64 modus_internus_adsignationem_semaphororum_mediorum_duplica()
{
    u64 new_count = 0;
    if(NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM == 0)
    { new_count = 1; }
    else
    { new_count = NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM * 2; }

    u64 new_page_count = ((sizeof(SemaphorumMedium) * new_count + PAGE_SIZE - 1) / PAGE_SIZE);

    Kallocation new_alloc = kalloc_pages(new_page_count);
    if(new_alloc.memory == 0) { return 0; }
    SemaphorumMedium* new_array = new_alloc.memory;
    SemaphorumMedium* old_array = NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM.memory;
    for(u64 i = 0; i < NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM; i++)
    {
        new_array[i] = old_array[i];
    }
    kfree_pages(NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM);
    NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM = new_alloc;

    // add new slots
    SemaphorumMedium empty_slot = {0};
    for(u64 i = NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM; i < new_count; i++)
    {
        new_array[i] = empty_slot;
    }
    NUCLEUS_MAGNITUDO_LINEAE_SEMAPHORORUM_MEDIORUM = new_count;

    return 1;
}

u64 semaphorum_medium_crea(u64* index_ad_ansam, s64 pretium_primum, s64 pretium_maximum)
{
    if( pretium_primum  > PRETIUM_MAXIMUM_ABSOLUTUM_SEMAPHORI_MEDII ||
        pretium_maximum > PRETIUM_MAXIMUM_ABSOLUTUM_SEMAPHORI_MEDII)
    {
        return 0;
    }

    rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);

    u64 ansa;

    if(!modus_internus_semaphorum_medium_crea(&ansa))
    {
        rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
        rwlock_acquire_write(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);

        if(!modus_internus_adsignationem_semaphororum_mediorum_duplica())
        {
            rwlock_release_write(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
            return 0;
        }
        assert(modus_internus_semaphorum_medium_crea(&ansa), "after doubling array we should be able to find a valid one");

        rwlock_release_write(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
        rwlock_acquire_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    }

    SemaphorumMedium* linea_semaphororum = NUCLEUS_ADSIGNATIO_LINEAE_SEMAPHORORUM_MEDIORUM.memory;
    atomic_s64_set(&linea_semaphororum[ansa].pretium_nunc, pretium_primum);
    linea_semaphororum[ansa].pretium_maximum = pretium_maximum;

    rwlock_release_read(&NUCLEUS_SLSERA_LINEAE_SEMAPHORORUM_MEDIORUM);
    *index_ad_ansam = ansa;
    return 1;
}
