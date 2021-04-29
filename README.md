dwm-enhanced - dynamic window manager enhanced
--------------------
dwm-enhanced is a customized version of suckless dwm that provides high level experience.


Requirements
------------
In order to build dwm-enhanced you need to have X installed on your system.

To have complete functionality while using dwm-enhanced, its better to have following dependencies aswell:

| dependency																		    | role                          |
| -									 								 				    | -                             |
| brightnessctl						 								 				    | brightness controlling        |
| lightdm							 								 				    | screen locking                |
| flameshot															 				    | screenshoting				    |
| st **[st-enhanced](https://github.com/ozkanonur/st-enhanced)**         				| terminal                      |
| dmenu  **[dmenu-enhanced](https://github.com/ozkanonur/dmenu-enhanced)**				| app launcher & cmd executer   |
| dwmblocks  **[dwmblocks-enhanced](https://github.com/ozkanonur/dwmblocks-enhanced)**  | modular statusbar             |

Installation
------------
Enter the following command to build and install dwm-enhanced:

    sudo make clean install

Configuration
------------
Config file is the source code itself! General options are defined in the config header

Shortcuts
------------
| command								    | role												                            |
| -											| -													                            |
| **alt + f1**								| open menu											                            |
| **alt + shift + return**					| open terminal										                            |
| **alt + b**								| hide statusbar									                            |
| **alt + [j, k]**							| focus next stack on 1 step [clockwise,counterclockwise]				        |
| **alt + shift + [j, k]**					| move stack position to [clockwise,counterclockwise]	                       	|
| **alt + shift + [t, f, m, b, c, g]**		| change tag's layout to [tiled,float,monocle,bottom,three-column,grid]			|
| **alt + [h, l]**							| [decrease,increase] master size						  		                |
| **alt + return**							| put focused stack into master				  		                            |
| **alt + shift + e**						| quit stack								  		                            |
| **alt + [-, +, =]**						| [decrease,increase, reset] gap size							  		        |
| **alt + [0-1-2..9]**					 	| switch to specified tag					  		                            |
| **alt + 0**								| preview all stacks of all tags						  		                |
| **alt + shift + [0-1-2..9]**				| move focused stack to specified tag		  		                            |
| **alt + shift + 0**						| embed focused stack to all tags			  		                            |
| **prtsc**									| use screenshot tool						  		                            |
| **alt + shift + l**						| lock screen								  		                            |
| **alt + [f7, f8]**						| [decrease,increase] screen brightness				                    		|
| **alt + shift + f12**						| quit dwm session									                            |

Uninstallation
------------
Enter the following command to uninstall dwm-enhanced completely from the machine:

    sudo make clean uninstall
