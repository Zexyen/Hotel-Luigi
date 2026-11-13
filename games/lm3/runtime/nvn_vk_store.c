/* The CONTAINER STORE of the game (included by runtime/nvn_vk_m4.c; not a build unit of its own): the game's shader
 * containers copied out of its data into `dir`, so prep_main can make their pipelines before the game draws with them.
 * While a build is under way the folder holds "store_building.txt"; a start that finds it builds again.
 * Returns 1 when the store was written. The stand-in has none: stages are then made as the game first uses them. */
static int vks_build_pkz(const char *dir) { (void)dir; return 0; }
