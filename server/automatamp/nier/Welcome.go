// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package nier

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type Welcome struct {
	_tab flatbuffers.Table
}

func GetRootAsWelcome(buf []byte, offset flatbuffers.UOffsetT) *Welcome {
	n := flatbuffers.GetUOffsetT(buf[offset:])
	x := &Welcome{}
	x.Init(buf, n+offset)
	return x
}

func GetSizePrefixedRootAsWelcome(buf []byte, offset flatbuffers.UOffsetT) *Welcome {
	n := flatbuffers.GetUOffsetT(buf[offset+flatbuffers.SizeUint32:])
	x := &Welcome{}
	x.Init(buf, n+offset+flatbuffers.SizeUint32)
	return x
}

func (rcv *Welcome) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *Welcome) Table() flatbuffers.Table {
	return rcv._tab
}

func (rcv *Welcome) Guid() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Welcome) MutateGuid(n uint64) bool {
	return rcv._tab.MutateUint64Slot(4, n)
}

func (rcv *Welcome) IsMasterClient() bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.GetBool(o + rcv._tab.Pos)
	}
	return false
}

func (rcv *Welcome) MutateIsMasterClient(n bool) bool {
	return rcv._tab.MutateBoolSlot(6, n)
}

func (rcv *Welcome) HighestEntityGuid() uint32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(8))
	if o != 0 {
		return rcv._tab.GetUint32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Welcome) MutateHighestEntityGuid(n uint32) bool {
	return rcv._tab.MutateUint32Slot(8, n)
}

func WelcomeStart(builder *flatbuffers.Builder) {
	builder.StartObject(3)
}
func WelcomeAddGuid(builder *flatbuffers.Builder, guid uint64) {
	builder.PrependUint64Slot(0, guid, 0)
}
func WelcomeAddIsMasterClient(builder *flatbuffers.Builder, isMasterClient bool) {
	builder.PrependBoolSlot(1, isMasterClient, false)
}
func WelcomeAddHighestEntityGuid(builder *flatbuffers.Builder, highestEntityGuid uint32) {
	builder.PrependUint32Slot(2, highestEntityGuid, 0)
}
func WelcomeEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
	return builder.EndObject()
}