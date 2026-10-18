/* Zephyr v4.3.0 template linker: explicit read-only ARM unwind tables.
 * Used only for C-only builds; CONFIG_CPP/CONFIG_RUST already place it.
 * Keep the output in the ROM group, not an implicitly placed orphan.
 */
SECTION_PROLOGUE(.ARM.extab,,)
{
  *(.ARM.extab* .gnu.linkonce.armextab.*)
} GROUP_LINK_IN(ROMABLE_REGION)
