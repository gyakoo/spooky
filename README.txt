Spooky Adulthood
================
Manu Marin
Please any question to:
mmrom@microsoft.com
This game is created for ScareJam2016, an initiative by Garage Gaming at Microsoft Inc.
It was created at home in spare time.
Ocasionally I worked on it a couple of lunch breaks at work.

October 31st 2016

GAME
====
* First Person Shooter
* Kill all enemies with treats and candies, room-by-room to open it up
* When the room opens, you can hear a sound and the doors turns green, you can pass thru.
* When all rooms are open, a boss will appear (see the dark red room in the minimap) and 
	you'll have to defeat it.


CONTROLS
========
* Mouse and Left-Click to look and shoot
* WASD 	- For movement

* F1 	- Help screen
* F5 	- Toggle Fullscreen
* Esc 	- Exit or Menu
* Tab 	- Toggle Minimap

ORIGINAL IDEAS
==============
* Originally it was going to be a light RPG where you fight against bourocracy and other issues as an adult.
* Due 5-minutes rule, I changed the design
* The second design was a FPS but more focused on Mental Illness enemies (for awareness). Original ways to kill those ones.
* Due time-constraint I changed a little de design as I wasn't sure if I was going to achieve a fun experience doing that.

TOOLS
=====
* Microsoft Visual Studio 2015 Community Edition
* C++ / DirectX11 / DirectXTK (I started from scratch using the DX Sample)
* The code does not use any lib or code from any other library, all was created from scratch.

POSTMORTEM
==========
Technical:
- It randomly generates all rooms using BSP trees. Randomly generate room-profiles.
- Only renders/update the room you are
- BSP Portals were generated but does not make use of it.
- Technically it is 2D (for collision/intersection against level).
- Entities move in 3D
- Spotlight effect achieved tweaking exp fog density depending on where pixel is
- 3 Pixel Shaders / 2 Vertex Shaders
- Sounds created with Bfxr + Freesounds.org (see authors.txt in Assets/Sounds/ folder)
- Uses DirectXTK lib for Audio, Input.

What was right:
- Reducing scope at right time
- Not overengineering
- Not making an engine but a game. Forgetting about internal code quality.
- Not data driven, all hardcoded. Iteration times were fast enough.

What was wrong:
- Everything from scratch (systems + gamepley)
- Design changes during development
- Random level generation. It took too much time specially make all rooms connected.
	- Sometimes there are room clusters disconnected and some teleports are created



KNOWN ISSUES
============
- Some rooms can be generated disconnected. It's a major issue, but happens few times. Restart then.
	- Algorithm to find disconnected clusters has some bug.
	
- Bad performance on Fullscreen on huge target sizes (4K)
	- Because the game RenderTarget is created out of actual Target and the PS is unoptimized	
	
	UPDATE: This is solved last minute fix (clamping to 1920x1080) but haven't been tested much :()

	
mmrom@microsoft.com


