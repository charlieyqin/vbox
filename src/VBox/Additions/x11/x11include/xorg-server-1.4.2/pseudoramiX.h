/*
 * Minimal implementation of PanoramiX/Xinerama
 */

extern int noPseudoramiXExtension;

void PseudoramiXAddScreen(int x, int y, int w, int h);
void PseudoramiXExtensionInit(int argc, char *argv[]);
void PseudoramiXResetScreens(void);
