# axaka-player

This is a player for the music files of Inside-cap's later GBA games, using their AXAKA sound engine. Basically just made to try out SDL.

All song/instrument pack files are contained within the `songs` folder. The Higurashi and Tsukihime ones are directly from the original CD-ROMs, the others are ripped from the game ROMs (Bittersweet Fools and Suika use a different sound engine and can't be ripped).

Requires SDL2, SDL2_gfx, and `make` to build.

To use it, just run the executable and drag and drop the instrument file (if there is one) onto the window, and then the song file. Press Space to restart the song and Escape to stop it.