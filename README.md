# ReverseJoy
Mouse and Keyboard to virtual controller input translator. This can give aim assist and axis heavy movement in certain games. <br>
This also can give keyboard and mouse support for games that only support controllers, such as emulated games. <br>
I recommend you fork and edit the code if you want something more customizable than this, as this is more just for my hardcoded personal use. <br>
This is really useful if you are a game developer that needs to test controller input for your game, and you don't have a controller on hand.

# How to use
Install vJoy driver, ViGem bus driver, and Interception driver: <br>
https://sourceforge.net/projects/vjoystick/ <br>
https://vigembusdriver.com/ <br>
https://github.com/oblitum/Interception <br>
Then download the ReverseJoy program in releases and make sure you have C++ redistrutable installed. <br>

# Controls
G to toggle active (off goes into normal mouse and keyboard input) <br>
J to kill switch the entire program <br>
H to toggle mouse override (so right stick and left/right triggers are bound to mouse) <br>
WASD to move left stick around for movement <br>
Space to press A button <br>
Q to press X button <br>
E to press B button <br>
1 to press left bumper <br>
2 to press Y button  <br>
3 to press right bumper  <br>
Mouse movement to control right stick to aim and look around <br>
Left click to do left trigger <br>
Right click to do right trigger  <br>

Extra binds to do later: <br>
Escape to do start button  <br>
Scroll wheel up or down for left and right bumper

# TO DO
1. Mouse input sucks right now, playable but not in the percision way you would want, it's too similar to a non precise analog joystick at the moment. <br>
To fix this problem I might make a more customizable sensitivity for each axis. I also probably just need to do some math and program smarter "autopilot" controls. <br>
2. Mouse clicks come in delayed and holding down left/right mouse button does not register properly. I think this is a report rate issue, especially since I am sending 2 reports from a mouse and keyboard seperately. <br>
Right now the best way to play most games using this is with the H key mouse override toggled off (see controls list)
