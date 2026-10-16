/* Native host driver for the same ABI stand-in used in ARM link checks.
 * Neither this test nor its ARM link proves .ores compiler execution. */
extern int cortexm_static_actor_link_smoke(void);
int main(void) { return cortexm_static_actor_link_smoke(); }
