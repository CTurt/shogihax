# shogihax
A remote code execution exploit against the game Morita Shogi 64 for the Nintendo 64. From CTurt and ppcasm.

Read more [here](https://cturt.github.io/shogihax.html).

Video demo of early homebrew channel proof-of-concept [here](https://www.youtube.com/watch?v=9VyoLYnWx7o).

## Install
There are some dependencies:

	sudo apt-get install python2
	sudo apt install python-pip
	sudo pip install sh
	sudo pip install pyserial

## Use
Compile your stage 3 payload (entry-point at offset `0` in the binary), and then run the below command:

    sudo python shogihax.py /dev/ttyACM0 payload/stage2.bin payload/stage3.bin

To trigger the exploit from the N64, select the bottom menu item, then the top menu item.

 You can also specify a custom stage 2 payload (exploit can be configured to use up to `0x1000` bytes for that stage), or just use the default, which downloads and executes stage 3 from `0x80000400`.
