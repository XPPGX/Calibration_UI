import can

bus = can.interface.Bus(channel='can0', bustype='socketcan')

print("Listenning")
while True:
	message = bus.recv()
	if message:
		print(f"Received message : {message}")
