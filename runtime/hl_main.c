/* hotel-luigi-runtime: the runtime is libhotelluigi.so beside this program (boot.c hl_host_main), so the whole of it can be
 * prebuilt; the launcher starts this when it cannot load the library itself. */
int hl_host_main(int argc, char **argv);
int main(int argc, char **argv) { return hl_host_main(argc, argv); }
