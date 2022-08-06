package main

import (
	nier "automatampserver/nier"
	"encoding/json"
	"os"
	"strconv"

	"github.com/codecat/go-enet"
	"github.com/codecat/go-libs/log"
	flatbuffers "github.com/google/flatbuffers/go"
)

type Client struct {
	guid           uint64
	model          uint32
	name           string
	isMasterClient bool
	lastPlayerData *nier.PlayerData
}

type Connection struct {
	peer   enet.Peer
	client *Client
}

func check(err error) {
	if err != nil {
		panic(err)
	}
}

func handlepanic() {
	if a := recover(); a != nil {
		log.Info("RECOVER", a)
	}
}

func checkValidPacket(data *nier.Packet) bool {
	// recovering from the panic will return false
	// so this should be fine
	// the reason for the panic handler is so some client
	// can't send us garbage data to crash the server.
	defer handlepanic()

	if data.Magic() != 1347240270 {
		log.Error("Invalid magic number: %d", data.Magic())
		return false
	}

	if data.Id() == 0 {
		log.Error("Invalid packet type: %d", data.Id())
		return false
	}

	return true
}

func packetStart(id nier.PacketType) *flatbuffers.Builder {
	builder := flatbuffers.NewBuilder(0)
	nier.PacketStart(builder)
	nier.PacketAddMagic(builder, 1347240270)
	nier.PacketAddId(builder, id)
	//nier.PacketEnd(builder)

	return builder
}

func makeVectorData(builder *flatbuffers.Builder, data []uint8) flatbuffers.UOffsetT {
	dataoffs := flatbuffers.UOffsetT(0)

	if len(data) > 0 {
		nier.PacketStartDataVector(builder, len(data))
		for i := len(data) - 1; i >= 0; i-- {
			builder.PrependUint8(data[i])
		}
		dataoffs = builder.EndVector(len(data))
	}

	return dataoffs
}

func packetStartWithData(id nier.PacketType, data []uint8) *flatbuffers.Builder {
	builder := flatbuffers.NewBuilder(0)

	dataoffs := makeVectorData(builder, data)

	nier.PacketStart(builder)
	nier.PacketAddMagic(builder, 1347240270)
	nier.PacketAddId(builder, id)

	if (len(data)) > 0 {
		nier.PacketAddData(builder, dataoffs)
	}
	//nier.PacketEnd(builder)

	return builder
}

func makePacketBytes(id nier.PacketType, data []uint8) []uint8 {
	builder := packetStartWithData(id, data)
	builder.Finish(nier.PacketEnd(builder))
	return builder.FinishedBytes()
}

func makeEmptyPacketBytes(id nier.PacketType) []uint8 {
	builder := packetStart(id)
	builder.Finish(nier.PacketEnd(builder))
	return builder.FinishedBytes()
}

func builderSurround(cb func(*flatbuffers.Builder) flatbuffers.UOffsetT) []uint8 {
	builder := flatbuffers.NewBuilder(0)
	offs := cb(builder)
	builder.Finish(offs)
	return builder.FinishedBytes()
}

func makePlayerPacketBytes(connection *Connection, id nier.PacketType, data []uint8) []uint8 {
	playerPacketData := builderSurround(func(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
		dataoffs := makeVectorData(builder, data)

		nier.PlayerPacketStart(builder)
		nier.PlayerPacketAddGuid(builder, connection.client.guid)
		nier.PlayerPacketAddData(builder, dataoffs)
		return nier.PlayerPacketEnd(builder)
	})

	return makePacketBytes(id, playerPacketData)
}

// client map
var connections = make(map[enet.Peer]*Connection)
var clients = make(map[*Connection]*Client)
var connectionCount uint64 = 0

func broadcastPacketToAll(id nier.PacketType, data []uint8) {
	broadcastData := makePacketBytes(id, data)

	for conn := range clients {
		conn.peer.SendBytes(broadcastData, 0, enet.PacketFlagReliable)
	}
}

func broadcastPacketToAllExceptSender(sender enet.Peer, id nier.PacketType, data []uint8) {
	broadcastData := makePacketBytes(id, data)

	for conn := range clients {
		if conn.peer == sender {
			continue
		}

		conn.peer.SendBytes(broadcastData, 0, enet.PacketFlagReliable)
	}
}

func broadcastPlayerPacketToAll(connection *Connection, id nier.PacketType, data []uint8) {
	broadcastData := makePlayerPacketBytes(connection, id, data)

	for conn := range clients {
		conn.peer.SendBytes(broadcastData, 0, enet.PacketFlagReliable)
	}
}

func broadcastPlayerPacketToAllExceptSender(sender enet.Peer, connection *Connection, id nier.PacketType, data []uint8) {
	broadcastData := makePlayerPacketBytes(connection, id, data)

	for conn := range clients {
		if conn.peer == sender {
			continue
		}

		conn.peer.SendBytes(broadcastData, 0, enet.PacketFlagReliable)
	}
}

func getFilteredPlayerName(input []uint8) string {
	out := ""

	if input == nil || string(input) == "" {
		log.Error("Client sent empty name, assigning random name")
		out = "Client" + strconv.FormatInt(int64(len(connections)), 10)
	} else {
		out = string(input)
	}

	for _, client := range clients {
		if client.name == out {
			log.Error("Client sent duplicate name, appending number")
			out = out + strconv.FormatInt(int64(len(connections)), 10)
			break
		}
	}

	return out
}

func main() {
	// Initialize enet
	enet.Initialize()

	// Create a host listening on 0.0.0.0:6969
	host, err := enet.NewHost(enet.NewListenAddress(6969), 32, 1, 0, 0)
	if err != nil {
		log.Error("Couldn't create host: %s", err.Error())
		return
	}

	log.Info("Created host")

	serverJson, err := os.ReadFile("server.json")
	if err != nil {
		log.Error("Server requires a server.json file to be present")
		return
	}

	var serverConfig map[string]interface{}
	json.Unmarshal(serverJson, &serverConfig)

	serverPassword := serverConfig["password"]
	log.Info("Server password: %s", serverPassword)

	// The event loop
	for true {
		// Wait until the next event
		ev := host.Service(1000)

		// Do nothing if we didn't get any event
		if ev.GetType() == enet.EventNone {
			continue
		}

		switch ev.GetType() {
		case enet.EventConnect: // A new peer has connected
			log.Info("New peer connected: %s", ev.GetPeer().GetAddress())
			connection := &Connection{}
			connection.peer = ev.GetPeer()
			connection.client = nil
			connections[ev.GetPeer()] = connection
			break

		case enet.EventDisconnect: // A connected peer has disconnected
			log.Info("Peer disconnected: %s", ev.GetPeer().GetAddress())
			if connections[ev.GetPeer()] != nil {
				isMasterClient := connections[ev.GetPeer()].client != nil && connections[ev.GetPeer()].client.isMasterClient

				// Broadcast a destroy player packet to everyone except the disconnected peer
				if connections[ev.GetPeer()].client != nil {
					destroyPlayerBytes := builderSurround(func(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
						return nier.CreateDestroyPlayer(builder, connections[ev.GetPeer()].client.guid)
					})

					broadcastPacketToAllExceptSender(ev.GetPeer(), nier.PacketTypeID_DESTROY_PLAYER, destroyPlayerBytes)
				}

				delete(clients, connections[ev.GetPeer()])

				if isMasterClient {
					// we must find a new master client
					for conn, client := range clients {
						log.Info("Setting new master client: %s @ %s", client.name, conn.peer.GetAddress())

						client.isMasterClient = true
						conn.peer.SendBytes(makeEmptyPacketBytes(nier.PacketTypeID_SET_MASTER_CLIENT), 0, enet.PacketFlagReliable)
						break
					}
				}
			}

			delete(connections, ev.GetPeer())
			break

		case enet.EventReceive: // A peer sent us some data
			connection := connections[ev.GetPeer()]

			if connection == nil {
				log.Error("Received data from unknown peer, ignoring")
				break
			}

			// Get the packet
			packet := ev.GetPacket()

			// We must destroy the packet when we're done with it
			defer packet.Destroy()

			// Get the bytes in the packet
			packetBytes := packet.GetData()

			if connection.client != nil {
				//log.Info("Peer %d @ %s sent data %d bytes", connection.client.guid, ev.GetPeer().GetAddress().String(), len(packetBytes))
			} else {
				log.Info("Peer %s sent data %d bytes", ev.GetPeer().GetAddress().String(), len(packetBytes))
			}

			data := nier.GetRootAsPacket(packetBytes, 0)

			if !checkValidPacket(data) {
				continue
			}

			if data.Id() != nier.PacketTypeID_HELLO && connection.client == nil {
				log.Error("Received packet before hello was sent, discarding")
				continue
			}

			switch data.Id() {
			case nier.PacketTypeID_HELLO:
				log.Info("Hello packet received")

				if connection.client != nil {
					log.Error("Received hello packet from client that already has a client, discarding")
					continue
				}

				helloData := nier.GetRootAsHello(data.DataBytes(), 0)

				if helloData.Major() != uint32(nier.VersionMajorValue) {
					log.Error("Invalid major version: %d", helloData.Major())
					ev.GetPeer().DisconnectNow(0)
					continue
				}

				if helloData.Minor() > uint32(nier.VersionMinorValue) {
					log.Error("Client is newer than server, disconnecting")
					ev.GetPeer().DisconnectNow(0)
					continue
				}

				if helloData.Patch() != uint32(nier.VersionPatchValue) {
					log.Info("Minor version mismatch, this is okay")
				}

				log.Info("Version check passed")

				if serverPassword != "" {
					if string(helloData.Password()) != serverPassword {
						log.Error("Invalid password, client sent: \"%s\"", string(helloData.Password()))
						ev.GetPeer().DisconnectNow(0)
						continue
					}
				}

				log.Info("Password check passed")

				if _, ok := nier.EnumNamesModelType[nier.ModelType(helloData.Model())]; !ok {
					log.Error("Invalid model type: %d", helloData.Model())
					ev.GetPeer().DisconnectNow(0)
					return
				}

				log.Info("Model type check passed")

				clientName := getFilteredPlayerName(helloData.Name())

				connectionCount++

				// Create a new client for the peer
				client := &Client{
					guid:  connectionCount,
					name:  clientName,
					model: helloData.Model(),

					// Allows the first person that connects to be the master client
					// In an ideal world, the server would run all of the simulation logic
					// like movement, physics, enemy AI & movement, but this would be
					// a monumental task because this is a mod, not a game where we have the source code.
					// So we let the master client control the simulation.
					isMasterClient: len(clients) == 0,
				}

				log.Info("Client name: %s", clientName)
				log.Info("Client GUID: %d", client.guid)
				log.Info("Client is master client: %t", client.isMasterClient)

				// Add the client to the map
				connection.client = client
				clients[connection] = client

				// Send a welcome packet
				welcomeBytes := builderSurround(func(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
					nier.WelcomeStart(builder)
					nier.WelcomeAddGuid(builder, client.guid)
					nier.WelcomeAddIsMasterClient(builder, client.isMasterClient)
					return nier.WelcomeEnd(builder)
				})

				log.Info("Sending welcome packet")
				ev.GetPeer().SendBytes(makePacketBytes(nier.PacketTypeID_WELCOME, welcomeBytes), 0, enet.PacketFlagReliable)

				// Send the player creation packet
				createPlayerBytes := builderSurround(func(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
					playerName := builder.CreateString(client.name)
					nier.CreatePlayerStart(builder)
					nier.CreatePlayerAddGuid(builder, client.guid)
					nier.CreatePlayerAddName(builder, playerName)
					nier.CreatePlayerAddModel(builder, client.model)
					return nier.CreatePlayerEnd(builder)
				})

				log.Info("Sending create player packet to everyone")
				broadcastPacketToAll(nier.PacketTypeID_CREATE_PLAYER, createPlayerBytes)

				// Broadcast previously connected clients to the new client
				for _, prevClient := range clients {
					if prevClient == nil || prevClient == client { // Skip the new client
						continue
					}

					createPlayerBytes := builderSurround(func(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
						playerName := builder.CreateString(prevClient.name)
						nier.CreatePlayerStart(builder)
						nier.CreatePlayerAddGuid(builder, prevClient.guid)
						nier.CreatePlayerAddName(builder, playerName)
						nier.CreatePlayerAddModel(builder, prevClient.model)
						return nier.CreatePlayerEnd(builder)
					})

					log.Info("Sending create player packet for previous client %d to client %d", prevClient.guid, client.guid)
					ev.GetPeer().SendBytes(makePacketBytes(nier.PacketTypeID_CREATE_PLAYER, createPlayerBytes), 0, enet.PacketFlagReliable)
				}
				break
			case nier.PacketTypeID_PING:
				log.Info("Ping received from %s", connection.client.name)

				ev.GetPeer().SendBytes(makeEmptyPacketBytes(nier.PacketTypeID_PONG), 0, enet.PacketFlagReliable)
				break
			case nier.PacketTypeID_PLAYER_DATA:
				//log.Info("Player data received")
				playerData := &nier.PlayerData{}
				flatbuffers.GetRootAs(data.DataBytes(), 0, playerData)

				/*log.Info(" Flashlight: %d", playerData.Flashlight())
				log.Info(" Speed: %f", playerData.Speed())
				log.Info(" Facing: %f", playerData.Facing())
				pos := playerData.Position(nil)
				log.Info(" Position: %f, %f, %f", pos.X(), pos.Y(), pos.Z())*/

				connection.client.lastPlayerData = playerData

				// Broadcast the packet back to all valid clients (except the sender)
				broadcastPlayerPacketToAllExceptSender(ev.GetPeer(), connection, nier.PacketTypeID_PLAYER_DATA, data.DataBytes())
				break
			case nier.PacketTypeID_ANIMATION_START:
				log.Info("Animation start received")

				animationData := &nier.AnimationStart{}
				flatbuffers.GetRootAs(data.DataBytes(), 0, animationData)

				log.Info("Animation: %d", animationData.Anim())
				log.Info("Variant: %d", animationData.Variant())
				log.Info("a3: %d", animationData.A3())
				log.Info("a4: %d", animationData.A4())

				// TODO: sanitize the data

				// Broadcast the packet back to all valid clients (except the sender)
				broadcastPlayerPacketToAllExceptSender(ev.GetPeer(), connection, nier.PacketTypeID_ANIMATION_START, data.DataBytes())
				break
			case nier.PacketTypeID_BUTTONS:
				log.Info("Buttons received")

				// Broadcast the packet back to all valid clients (except the sender)
				broadcastPlayerPacketToAllExceptSender(ev.GetPeer(), connection, nier.PacketTypeID_BUTTONS, data.DataBytes())
				break
			case nier.PacketTypeID_SPAWN_ENTITY:
				log.Info("Spawn entity received")
				if !connection.client.isMasterClient {
					log.Info(" Not a master client, ignoring")
					break
				}

				broadcastPacketToAllExceptSender(ev.GetPeer(), nier.PacketTypeID_SPAWN_ENTITY, data.DataBytes())
				break
			case nier.PacketTypeID_DESTROY_ENTITY:
				log.Info("Destroy entity received")
				if !connection.client.isMasterClient {
					log.Info(" Not a master client, ignoring")
					break
				}

				broadcastPacketToAllExceptSender(ev.GetPeer(), nier.PacketTypeID_DESTROY_ENTITY, data.DataBytes())
				break
			case nier.PacketTypeID_ENTITY_DATA:
				log.Info("Entity data received")
				if !connection.client.isMasterClient {
					log.Info(" Not a master client, ignoring")
					break
				}

				broadcastPacketToAllExceptSender(ev.GetPeer(), nier.PacketTypeID_ENTITY_DATA, data.DataBytes())
				break
			default:
				log.Error("Unknown packet type: %d", data.Id())
			}
		}
	}

	// Destroy the host when we're done with it
	host.Destroy()

	// Uninitialize enet
	enet.Deinitialize()
}
