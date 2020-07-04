import aiohttp
import asynchio
import json
import sys
import time


Places_API_Key = "AIzaSyAsBtnES7fWMz-ACnGOZWCzXRx4S65Unak"

server_nodes = ['Hill', 'Jaquez', 'Smith', 'Campbell', 'Singleton']
ports_dict = { 'Hill': 12585, 'Jaquez': 12586, 'Smith': 12587, 'Campbell': 12588, 'Singleton': 12589 }
server_edges = { 'Hill': ['Jaquez', 'Smith'], 
				 'Singleton': ['Jaquez', 'Smith', 'Campbell'], 
				 'Smith': ['Campbell'] 
				}

clients = {}

'''
async def iamat_message(client, location, client_time, server_time):
	(lat, lon) = parse_location(location)
	time_difference = float(server_time) - float(client_time)


	clients[client] = {
		'server': server_name
		'latitude': lat,
		'longitude': lon,
		'time_diff': time_difference,
		'client_time': client_time
	}

	#need to do processing to determine + or - in front


	response = 'AT %s %s %s %s' % (server_name, time_difference, client, location, client_time)
	return response
'''

'''
	TO DO: HOW TO FLOOD INFO TO OTHER SERVERS?
	NEW MESSAGE FORMAT?
	USE AT MESSAGES FROM SERVERS?

'''

def parse_location(location):
	second = 0
	index = 0
	for i in location: 
		if i == '+' or i == '-':
			second += 1
			if second == 2:
				lat = location[:(index-1)]
				lon = location[index:]
		index += 1

	return (lat, lon)

#asynch method called until keyboard interrupted
#reads input from reader and outputs to writer
def client_connected_cb(reader, writer):

	#read line from connected client
	data= await reader.readline()

	#change from network to host
	message_raw = data.decode()
	#record time
	message_time = time.time()

	#report connection on log file
	log_file.write("RECEIVED: " + message_raw)


	'''
	Server can recieve 2 types of messages from client:

	IAMAT - 4 parts
		IAMAT <clientID> <GPSlocation> <clientTime>

	WHATSAT <otherClientID> <Radius(KM)> <amountOfInfo>
		Radius <= 50KM
		amountOfInfo <= 20 items
	'''

	output_message = await get_output(message_raw, message_time)

	if output_message == None:
		#log server rsponse
		log_file.write("SENDING: " + output_message)

		#server response sent to client
		writer.write(output_message.encode())




async def get_output(server_name, rawMessage, mtime):
	#parse the message
	msg_parsed = rawMessage.strip().split(' ')
	if len(msg_parsed) == 4:

		#IAMAT kiwi.cs.ucla.edu +34.068930-118.445127 1520023934.918963997
		if msg_parsed[0] == "IAMAT":

			client = msg_parsed[1]
			(lat, lon) = parse_location(msg_parsed[2])
			time_difference = float(mtime) - float(msg_parsed[3])

			if time_difference > 0:
				time_difference = '+' + str(time_difference)

			clients[client] = {
				'server': server_name,
				'latitude': lat,
				'longitude': lon,
				'time_diff': time_difference,
				'client_time': client_time
			}

			output_message = 'AT %s %s %s %s' % (server_name, time_difference, client, location, client_time)

			#need to change
			asyncio.ensure_future(flood('FLOOD %s %s %s %s' % (client, msg_parsed[2], time_difference, client_time), server_name))

			return output_message
		#WHATSAT kiwi.cs.ucla.edu 10 5
		#AT Hill +0.263873386 kiwi.cs.ucla.edu +34.068930-118.445127 1520023934.918963997
		elif msg_parsed[0] = "WHATSAT":
			client = msg_parsed[1]
			radius = int(msg_parsed[2])
			numItems = int(msg_parsed[3])
			#radius and number of items returned limited due to Google Places API
			if radius >= 0 && radius <= 50 && numItems >= 0 && numItems <= 20
				
				if client not in clients
					#output_message = error
				else:
					ctime = clients[client]['client_time']
					time_difference = clients[client]['time_diff']
					location = clients[client]['latitude'] + clients[client]['longitude']
					url = 'https://maps.googleapis.com/maps/api/place/nearbysearch/json?key={0}&location={1}&radius={2}'.format(API_KEY, location, radius*1000)
					
					output_message = 'AT %s %s %s %s' % (server_name, time_difference, client, location, ctime)
					#flood()?
					async with aiohttp.ClientSession() as sess:
						async with sess.get(url) as res:
							response = await res.json()
							places = response['results'][:numItems]
							
							output_message += json.dumps(places, indent=3)

					return output_message

			else:
				#if the radius or number of items is not valid
				output_message = '? %s' % (rawMessage)
		else: output_message = '? %s' % (rawMessage)

	#FLOOD <client> <location> <time difference> <client time>
	elif len(msg_parsed) == 5 && msg_parsed[0] == 'FLOOD':
			client = msg_parsed[1]
			(lat, lon) = parse_location(msg_parsed[2])
			time_difference = msg_parsed[3]
			client_time = msg_parsed[4]

			if client not in clients:
				clients[client] = {
					'server': server_name,
					'latitude': lat,
					'longitude': lon,
					'time_diff': time_difference,
					'client_time': client_time
				}
				asyncio.ensure_future(flood('FLOOD %s %s %s %s' % (client, msg_parsed[2], time_difference, client_time), server_name))

			return None

	else:
		#not valid input
		output_message = '? %s' % (rawMessage)





async def flood(message, server_name):
	#for every server this server is connected to
	#send message

	for server in server_edges[server_name]:
		log_file.write("Connecting to server %s:%s.\n" % (server, ports_dict[server]))
		try:
			reader,writer = await asyncio.open_connection('127.0.0.1', ports_dict[server], loop=loop)
			log_file.write("Connected to server %s.\n" % (server))
			writer.write(message.encode())
			await writer.drain()
			writer.close()
		except:
			log_file.write("Failed to connect to server %s.\n" % (server))
			pass



def main():
	if(len(sys.argv) != 2):
		print("Invalid number of arguments: ")
		exit(1)

	global server_name
	global loop
	global log_file


	if sys.argv[1] not in ports_dict:
		print("Invalid server name.")
		exit(1)

	server_name = sys.argv[1]
	server_port = ports_dict[server_name]


	#set up log file
	log_file = open(server_name+"_log.txt", "w")


	#get_event_loop
	#if no current event loop, and set_event_loop not called
	#create new event loo-p and set as current
	loop = asynchio.get_event_loop()

	#calls client_conntected_cb whenever a client connects
	coro = asyncio.start_server(client_connected_cb, '127.0.0.1', server_port, loop=loop)
	server = loop.run_until_complete(coro)
	

	try:
		loop.run_forever()
	except KeyboardInterrupt:
		pass

	server.close()

	#run until the close() method completes
	loop.run_until_complete(server.wait_closed())
	loop.close()

	log_file.close()

if __name__ == '__main__':
	main()














