
<img width="1779" height="1051" alt="zzzzz" src="https://github.com/user-attachments/assets/0b78cc47-6395-4dfa-b2d9-518b7bba640d" />





Moki vs. Avocados

Bruh this aint no boring ass tech demo.
This is Moki vs. homicidal Avocados a fast, crunchy 2D action sandbox where you can

Jump / double jump / flip like a cracked out gymnast

Swing on webs and launch yourself across space

Build your own neon platforms mid run, cuz why not

Yeet spinning knives straight into Avocados that split when you slice em

Toggle Moon Gravity if you wanna feel floaty as hell

Its all physics driven, juicy :P

The whole things coded in pure C++20 with SDL2, and it feels way smoother than it has any right to.

🎮 Quick Controls Keys:

A / D -> Move

Space -> Jump / Double Jump

T -> Toggle off on for inifinite knives or recharge throwing

B / V -> Backflip / Frontflip (air only)

O -> Spawns avocado enemy (must spawn manually, when ever you want :P)

LMB -> Throw knife

RMB -> Web hook a tile (hold to swing, release to launch)

Hold N + RMB -> Build a neon platform under cursor

E / Q -> Reel in/out

Shift -> Pump the swing for more juice

R -> Restart when you get bodied

Esc -> Quit (coward’s move)

⚡ What to Expect

Youre out here in space with just your flips, your rope, and some knives. Avocados chase you down, and every hit takes a chunk out of your health bar. Survive by moving smart, building your own routes, and slicing anything green that comes at you.

Avocados dont play fair they split when you kill them. But that’s just more targets to flex on.

⚡ Controls (short list)
Move -> A/D
Jump -> Space (double jump)
Flip -> B back / V front (air only)
Knife -> LMB
Web -> RMB latch / release
Reel in/out -> E/Q
Pump swing -> Shift
Fine reel -> Wheel
Platform -> Hold N + RMB
Spawn Enemys -> O
System -> F fullscreen, M moon mode, T infinite knives, R restart, Esc quit

📊 HUD
HP bar (top left)
Knives (ammo + recharge)
Web status (None / Shooting / Latched)
Anchor UV (adjust with I/J/K/L)  This adjsuts on where webs are spawn, what area of the avatar can start a web spawn.

🛠 Build & Run
Deps: SDL2, SDL2_image, SDL2_ttf, SDL2_mixer

g++ -std=c++20 -Wall -Wextra -pedantic main.cpp -o app \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer
./app

Assets go in game/assets/... and game/nft/nft8.png (Moki sprite).

⚡ License: experimental / personal. Swing wild, jump, slice styled.
