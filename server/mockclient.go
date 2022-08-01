package main

import (
	"automatampserver/Nier"
	"math/rand"
	"time"

	"github.com/codecat/go-enet"
	"github.com/codecat/go-libs/log"
	flatbuffers "github.com/google/flatbuffers/go"
)

func checkValidPacket(data *Nier.Packet) bool {
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

func packetStart(id Nier.PacketType) *flatbuffers.Builder {
	builder := flatbuffers.NewBuilder(0)
	Nier.PacketStart(builder)
	Nier.PacketAddMagic(builder, 1347240270)
	Nier.PacketAddId(builder, id)
	//Nier.PacketEnd(builder)

	return builder
}

func packetStartWithData(id Nier.PacketType, data []uint8) *flatbuffers.Builder {
	builder := flatbuffers.NewBuilder(0)

	dataoffs := flatbuffers.UOffsetT(0)

	if len(data) > 0 {
		Nier.PacketStartDataVector(builder, len(data))
		for i := len(data) - 1; i >= 0; i-- {
			builder.PrependUint8(data[i])
		}
		dataoffs = builder.EndVector(len(data))
	}

	Nier.PacketStart(builder)
	Nier.PacketAddMagic(builder, 1347240270)
	Nier.PacketAddId(builder, id)

	if (len(data)) > 0 {
		Nier.PacketAddData(builder, dataoffs)
	}
	//Nier.PacketEnd(builder)

	return builder
}

func makePacketBytes(id Nier.PacketType, data []uint8) []uint8 {
	builder := packetStartWithData(id, data)
	builder.Finish(Nier.PacketEnd(builder))
	return builder.FinishedBytes()
}

func makeEmptyPacketBytes(id Nier.PacketType) []uint8 {
	builder := packetStart(id)
	builder.Finish(Nier.PacketEnd(builder))
	return builder.FinishedBytes()
}

func builderSurround(cb func(*flatbuffers.Builder) flatbuffers.UOffsetT) []uint8 {
	builder := flatbuffers.NewBuilder(0)
	offs := cb(builder)
	builder.Finish(offs)
	return builder.FinishedBytes()
}

func sendPing(peer enet.Peer) {
	peer.SendBytes(makeEmptyPacketBytes(Nier.PacketTypeID_PING), 0, enet.PacketFlagReliable)
}

func getNextPacket(ev enet.Event) *Nier.Packet {
	if ev.GetType() == enet.EventReceive {
		packet := ev.GetPacket()
		defer packet.Destroy()

		// Get the bytes in the packet
		packetBytes := packet.GetData()

		log.Info("Received %d bytes from server", len(packetBytes))

		data := Nier.GetRootAsPacket(packetBytes, 0)

		if !checkValidPacket(data) {
			return nil
		}

		return data
	}

	return nil
}

func sendHello(client enet.Host, peer enet.Peer) bool {
	sendPing(peer)

	for i := 0; i < 20; i++ {
		ev := client.Service(100)
		data := getNextPacket(ev)

		if data == nil {
			return false
		}

		if data.Id() == Nier.PacketTypeID_PONG {
			log.Info("Hello acknowledged")
			return true
		}
	}

	return false
}

func performStartupHandshake(client enet.Host, peer enet.Peer) bool {
	log.Info("Performing startup handshake")

	hasConnection := false

	for i := 0; i < 10; i++ {
		ev := client.Service(1000)

		if ev.GetType() == enet.EventConnect {
			log.Info("Intial connection established")
			hasConnection = true
			break
		}

		if ev.GetType() == enet.EventDisconnect {
			log.Error("Initial connection failed")
			return false
		}
	}

	if !hasConnection {
		log.Error("Initial connection failed")
		return false
	}

	log.Info("Sending initial hello...")
	if !sendHello(client, peer) {
		log.Error("Failed to receive hello response from server")
		return false
	}

	log.Info("Connected.")

	return true
}

func main() {
	// Initialize enet
	enet.Initialize()

	// Create a client host
	client, err := enet.NewHost(nil, 1, 1, 0, 0)
	if err != nil {
		log.Error("Couldn't create host: %s", err.Error())
		return
	}

	// Connect the client host to the server
	peer, err := client.Connect(enet.NewAddress("127.0.0.1", 6969), 1, 0)
	if err != nil {
		log.Error("Couldn't connect: %s", err.Error())
		return
	}

	if !performStartupHandshake(client, peer) {
		return
	}

	pingTime := time.Now()
	sendUpdateTime := time.Now()
	once_test := true

	// The event loop
	for true {
		now := time.Now()

		// Wait until the next event
		ev := client.Service(1000)

		// Send a ping if we didn't get any event
		if ev.GetType() == enet.EventNone {
			if once_test {
				log.Info("Sending animation start")
				animationStartBytes := builderSurround(func(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
					return Nier.CreateAnimationStart(builder, 1, 2, 3, 4)
				})

				animationData := makePacketBytes(Nier.PacketTypeID_ANIMATION_START, animationStartBytes)
				peer.SendBytes(animationData, 0, enet.PacketFlagReliable)

				once_test = false
			}

			if now.Sub(pingTime) > time.Second {
				log.Info("Sending ping")

				sendPing(peer)
				pingTime = now
				continue
			}

			if now.Sub(sendUpdateTime) >= (time.Second / 60) {
				//log.Info("Sending update")

				playerDataBytes := builderSurround(func(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
					return Nier.CreatePlayerData(builder, true, 0.1, 0.5, 0.25, 1, 0, 0, rand.Float32(), rand.Float32(), 250.0)
				})

				packetData := makePacketBytes(Nier.PacketTypeID_PLAYER_DATA, playerDataBytes)
				peer.SendBytes(packetData, 0, enet.PacketFlagReliable)
				sendUpdateTime = now
				continue
			}
		}

		switch ev.GetType() {
		case enet.EventConnect: // We connected to the server
			log.Info("Connected to the server!")

		case enet.EventDisconnect: // We disconnected from the server
			log.Info("Lost connection to the server!")

		case enet.EventReceive: // The server sent us data
			packet := ev.GetPacket()
			defer packet.Destroy()

			// Get the bytes in the packet
			packetBytes := packet.GetData()

			log.Info("Received %d bytes from server", len(packetBytes))

			data := Nier.GetRootAsPacket(packetBytes, 0)

			if !checkValidPacket(data) {
				continue
			}

			switch data.Id() {
			case Nier.PacketTypeID_PONG:
				log.Info("Pong received")
				break
			default:
				log.Error("Unknown packet type: %d", data.Id())
			}

		}
	}

	// Destroy the host when we're done with it
	client.Destroy()

	// Uninitialize enet
	enet.Deinitialize()
}
