// Copyright -> Scott Bishel

#include "MidiPrivatePCH.h"
#include "MidiComponent.h"

#include "MidiUtils.h"
#include "MidiFile.h"
#include "Util/MidiUtil.h"

#include "Event/ChannelAftertouch.h"
#include "Event/ChannelEvent.h"
#include "Event/Controller.h"
#include "Event/NoteAftertouch.h"
#include "Event/NoteOff.h"
#include "Event/NoteOn.h"
#include "Event/PitchBend.h"
#include "Event/ProgramChange.h"
#include "Event/SystemExclusiveEvent.h"
#include "Event/Meta/Tempo.h"

#include "Interface/ShortMessege.h"


#include "MidiAsset.h"

#include "Util/MidiProcessor.h"

// Sets default values for this component's properties
UMidiComponent::UMidiComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsBeginPlay = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.01f;
	// ...
	mProcessor.setListener(this);
	mMidiFile = NULL;
}

UMidiComponent::~UMidiComponent() {
	if (mMidiFile)
		delete mMidiFile;
	mMidiFile = NULL;
}


// Called when the game starts
void UMidiComponent::BeginPlay()
{
	Super::BeginPlay();
	// ...
}


// Called every frame
void UMidiComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );
		
	//mProcessor.process();
}


void UMidiComponent::ProcessTo(float time)
{
	mProcessor.processTo(time);
}

void UMidiComponent::GetMidiNotes(TArray<FMidiNote>& notes)
{
	if (!mMidiFile) 
		return;

	int ntracks = mMidiFile->getTrackCount();

	notes.Empty();

	int mMPQN = Tempo::DEFAULT_MPQN;
	int mPPQ = mMidiFile->getResolution();


	MidiTrack& trkTempo = *mMidiFile->getTracks()[0];
	TArray<MidiEvent*>::TIterator itTempo = trkTempo.getEvents().CreateIterator();

	TArray<Tempo*> tempoEvents;

	while (itTempo)
	{
		MidiEvent* _event = *itTempo;
		if (_event->getType() == MetaEvent::TEMPO)
		{
			Tempo* tempoEvent = (Tempo*)_event;
			//mMPQN = tempoEvent->getMpqn();
			tempoEvents.Add(tempoEvent);

		}
		itTempo++;
	}


	for (int i = 1; i < ntracks; i++)
	{
		MidiTrack& trk = *mMidiFile->getTracks()[i];
		TArray<MidiEvent*>::TIterator it = trk.getEvents().CreateIterator();
		TArray<FMidiNote> openNotes;

		int nextTempoId = -1;
		long nextTempoTick = -1;

		long startTick = 0;
		float startTime = 0.0f;

		if (tempoEvents.Num() > 0)
		{
			nextTempoId = 0;
			nextTempoTick = tempoEvents[nextTempoId]->getTick();
		}


		int channel = -1;

		while (it)
		{
			MidiEvent* _event = *it;

			////Tempo is supposed to come before everything
			//if (_event->getType() == MetaEvent::TEMPO)
			//{
			//	Tempo* tempoEvent = (Tempo*)_event;
			//	mMPQN = tempoEvent->getMpqn();
			//	UE_LOG(LogTemp, Warning, TEXT("Tempo %d %d, track %d!"), mPPQ, mMPQN,i);
			//}

			while (nextTempoTick>-1 && _event->getTick() >= nextTempoTick)
			{

				startTime = startTime + (((double)(nextTempoTick - startTick) * (double)mMPQN) / (double)mPPQ) / 1000000.0f;
				startTick = nextTempoTick;
				mMPQN = tempoEvents[nextTempoId]->getMpqn();

				//UE_LOG(LogTemp, Warning, TEXT("Tempo %d, time %f, track %d!"), mMPQN, startTime, i);

				nextTempoId++;

				if (nextTempoId < tempoEvents.Num())
				{
					nextTempoTick = tempoEvents[nextTempoId]->getTick();
				}
				else
				{
					nextTempoId = -1;
					nextTempoTick = -1;
				}
			}


			if (_event->getType() == ChannelEvent::NOTE_ON)
			{
				NoteOn* note = static_cast<NoteOn*>(_event);

				if (channel != note->getChannel())
				{
					channel = note->getChannel();
					UE_LOG(LogTemp, Warning, TEXT("Track %d, channel %d!"), i, channel);
				}
				if (note->getVelocity() > 0)
				{
					float start = startTime + (((double)(_event->getTick() - startTick) * (double)mMPQN) / (double)mPPQ) / 1000000.0f;
					openNotes.Add(FMidiNote(start, note->getNoteValue(), note->getChannel()));
				}
				else
				{
					float end = startTime + (((double)(_event->getTick() - startTick) * (double)mMPQN) / (double)mPPQ) / 1000000.0f;

					int pos = openNotes.Find(FMidiNote(0, note->getNoteValue(), note->getChannel()));
					if (pos != INDEX_NONE)
					{
						FMidiNote mNote = openNotes[pos];
						openNotes.RemoveAt(pos);
						mNote.Length = end - mNote.Start;
						notes.Add(mNote);
					}
				}

			}
			else if (_event->getType() == ChannelEvent::NOTE_OFF)
			{
				NoteOff* note = static_cast<NoteOff*>(_event);

				float end = startTime + (((double)(_event->getTick() - startTick) * (double)mMPQN) / (double)mPPQ) / 1000000.0f;

				int pos = openNotes.Find(FMidiNote(0, note->getNoteValue(), note->getChannel()));
				if (pos != INDEX_NONE)
				{
					FMidiNote mNote = openNotes[pos];
					openNotes.RemoveAt(pos);
					mNote.Length = end - mNote.Start;
					notes.Add(mNote);
				}
			}


			it++;

		}
	}

	notes.Sort();
}

void UMidiComponent::LoadAsset(UMidiAsset* MidiAsset) {
	if (mProcessor.isRunning()) return;

	if (mMidiFile)
		delete mMidiFile;
	mMidiFile = NULL;

	if (!MidiAsset) return;
	const TArray<uint8>& data = MidiAsset->Data;
	if (data.Num() == 0)
		return;

	FBufferReader reader((uint8*)data.GetData(), data.Num(), false);
	mMidiFile = new MidiFile(reader);
	mProcessor.load(*mMidiFile);
}

void UMidiComponent::LoadFile(FString path) {
	if (mProcessor.isRunning()) return;

	if (mMidiFile)
		delete mMidiFile;
	mMidiFile = NULL;
	
	TArray<uint8> data;
	bool result = FFileHelper::LoadFileToArray(data, path.GetCharArray().GetData());
	if (result == 0 || data.Num() == 0)
		return;

	FBufferReader reader((uint8*)data.GetData(), data.Num(), false);
	mMidiFile = new MidiFile(reader);
	mProcessor.load(*mMidiFile);
}

void UMidiComponent::onEvent(MidiEvent* _event) {
	if (_event->getType() == (ChannelEvent::NOTE_ON & 0x0F)) {
		NoteOn* note = static_cast<NoteOn*>(_event);
		OnEvent.Broadcast(note->getChannel(), note->getNoteValue(), note->getVelocity(), _event->getTick());
		//if (note->getChannel() == 7) {
		//	UE_LOG(LogTemp, Warning, TEXT("Note %ld, time %f!"), _event->getTick(), ((_event->getTick() * 454545.0) / 120.0) / 1000000.0);
		//}
	}

	else if (_event->getType() == (ChannelEvent::NOTE_OFF & 0x0F)) {
		NoteOff* note = static_cast<NoteOff*>(_event);
		OnEvent.Broadcast(note->getChannel(), note->getNoteValue(), 0, _event->getTick());
	}

	// MIDI Interface data
	if (_event->getType() >= ChannelEvent::NOTE_OFF && _event->getType() <= ChannelEvent::PITCH_BEND) {
		TArray<uint8> data;
		ShortMessage* shortMsg = static_cast<ShortMessage*>(_event);
		data.Add(shortMsg->getStatus());
		data.Add(shortMsg->getData1());
		data.Add(shortMsg->getData2());
		OnSend.Broadcast(data, _event->getTick());
	}
	else if (_event->getType() == 0xF0 || _event->getType() == 0xF7) {
	}
}

void UMidiComponent::onStart(bool fromBeginning) { OnStart.Broadcast(fromBeginning); }
void UMidiComponent::onStop(bool finish) { OnStop.Broadcast(finish); }

//-----------------------------------

void UMidiComponent::start() {
	mProcessor.start();
}

void UMidiComponent::stop() {
	mProcessor.stop();
}

void UMidiComponent::reset() {
	mProcessor.reset();
}

bool UMidiComponent::isStarted() {
	return mProcessor.isStarted();
}
bool UMidiComponent::isRunning() {
	return mProcessor.isRunning();
}

int UMidiComponent::GetResolution()
{
	if (mMidiFile)
	{
		return mMidiFile->getResolution();
	}
	else
	{
		return 0;
	}
}


