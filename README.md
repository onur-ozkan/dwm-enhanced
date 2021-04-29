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
| command								    | role												|
| -											| -													|
| **alt + f1**								| open menu											|
| **alt + shift + return**					| open terminal										|
| **alt + b**								| hide statusbar									|
| **alt + j**								| focus next stack on 1 step clockwise				|
| **alt + k**								| focus next stack on 1 step  counterclockwise		|
| **alt + shift +j**						| move stack position to clockwise					|
| **alt + shift +k**						| move stack position to counterclockwise     		|
| **alt + shift +t**						| change tag's layout to tiled						|
| **alt + shift +f**						| change tag's layout to float						|
| **alt + shift +m**						| change tag's layout to monocle					|
| **alt + shift +b**						| change tag's layout to bottom						|
| **alt + shift +c**						| change tag's layout to three-column		  		|
| **alt + shift +g**						| change tag's layout to grid			     		|
| **alt + l**								| increase master size						  		|
| **alt + h**								| decrease master size						  		|
| **alt + return**							| put focused stack into master				  		|
| **alt + shift + e**						| quit stack								  		|
| **alt + -**								| decrease gap size							  		|
| **alt + +**								| increase gap size							  		|
| **alt + shift + =**						| reset gap size							  		|
| **alt + [0-1-2..9]**					 	| switch to specified tag					  		|
| **alt + 0**								| preview all stacks						  		|
| **alt + shift + [0-1-2..9]**				| move focused stack to specified tag		  		|
| **alt + shift + 0**						| embed focused stack to all tasg			  		|
| **prtsc**									| use screenshot tool						  		|
| **alt + shift + l**						| lock screen								  		|
| **alt + f7**								| decrease screen brightness						|
| **alt + f8**								| increase screen brightness						|
| **alt + shift + f12**						| quit dwm session									|

Uninstallation
------------
Enter the following command to uninstall dwm-enhanced completely from the machine:

    sudo make clean uninstall
