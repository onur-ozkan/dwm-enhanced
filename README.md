dwm-enhanced - dynamic window manager enhanced
--------------------
dwm-enhanced is a customized version of suckless dwm that provides high level experience.


Requirements
------------
In order to build dwm-enhanced you need to have X installed on your system.

To have complete functionality while using dwm-enhanced, please install [bin scripts](https://github.com/ozkanonur/dotfiles/tree/master/.local/bin) and the following dependencies:

| dependency																		    | role                          |
| -									 								 				    | -                             |
| brightnessctl(for laptops)						 								 	| brightness controlling        |
| setxkbmap							 								 				    | keyboard layout switcher      |
| flameshot															 				    | screenshoting				    |
| **[st-enhanced](https://github.com/ozkanonur/st-enhanced)**         				    | terminal                      |
| **[dmenu-enhanced](https://github.com/ozkanonur/dmenu-enhanced)**				        | app launcher & cmd executer   |
| **[slock-enhanced](https://github.com/ozkanonur/slock-enhanced)**         		    | lockscreen                    |
| **[dwmblocks-enhanced](https://github.com/ozkanonur/dwmblocks-enhanced)**             | modular statusbar             |
| **[sbs](https://github.com/ozkanonur/sbs)**                                           | background setter             |


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
| **ctrl + return**							| open dmenu											                        |
| **ctrl + space**							| search on browser											                    |
| **alt + f**								| toggle fullscreen                     									    |
| **alt + tab**								| switch between previous-current tag                     						|
| **alt + space**							| switch keyboard layout between us/tr											|
| **alt + shift + return**					| open terminal										                            |
| **alt + b**								| hide statusbar									                            |
| **alt + [j, k]**							| focus next stack in 1 [clockwise,counterclockwise] step				        |
| **alt + [period, comma]**					| focus [forward,backward] monitor                                              |
| **alt + shift + [period, comma]**			| move window to [forward,backward] monitor                                     |
| **alt + shift + [j, k]**					| move stack position to [clockwise,counterclockwise]	                       	|
| **alt + shift + [t, f]**		            | change tag's layout to [tiled,float]											|
| **alt + [h, l]**							| [decrease,increase] master size						  		                |
| **alt + return**							| put focused stack into master				  		                            |
| **alt + shift + e**						| quit stack								  		                            |
| **alt + [-, +, =]**						| [decrease,increase,reset] gap size							  		        |
| **alt + [0-1-2..9]**					 	| switch to specified tag					  		                            |
| **alt + 0**								| preview all stacks of all tags						  		                |
| **alt + shift + [0-1-2..9]**				| move focused stack to specified tag		  		                            |
| **alt + shift + 0**						| embed focused stack to all tags			  		                            |
| **alt + shift + l**						| lock screen								  		                            |
| **alt + [f7, f8]**						| [decrease,increase] screen brightness				                    		|
| **alt + shift + f7**						| change window background									                    |
| **alt + shift + f12**						| quit dwm session									                            |
| **alt + shift + p**						| use screenshot tool						  		                            |
| **prtsc**									| use screenshot tool						  		                            |

Uninstallation
------------
Enter the following command to uninstall dwm-enhanced completely from the machine:

    sudo make clean uninstall


Related article: https://onurozkan.dev/posts/reasons-that-i-left-gnome-and-kde
