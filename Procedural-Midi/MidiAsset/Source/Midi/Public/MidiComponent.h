// Copyright -> Scott Bishel

#pragma once

#include "MidiFile.h"
#include "Event/MidiEvent.h"
#include "Util/MetronomeTick.h"
#include "Util/MidiProcessor.h"

#include "Components/ActorComponent.h"
#include "MidiComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEventStart, bool, beginning);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEventStop, bool, finished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FEventMidi, int32, track,int32, note,int32, velocity,int32, elapsed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEventDevice, TArray<uint8>, msg, int32, elapsed);

USTRUCT(BlueprintType)
struct FMidiNote
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(BlueprintReadWrite)
		float Start;

	UPROPERTY(BlueprintReadWrite)
		float Length;

	UPROPERTY(BlueprintReadWrite)
		int Value;

	UPROPERTY(BlueprintReadWrite)
		int Channel;

	FMidiNote()
	{
		Start = 0.0f;
		Length = 0.0f;
		Value = 0;
		Channel = 0;
	}

	FMidiNote(float astart, int avalue, int achannel) :FMidiNote()
	{
		Start = astart;
		Length = 0.0f;
		Value = avalue;
		Channel = achannel;
	}

	FORCEINLINE bool operator==(const FMidiNote &Other) const
	{
		return Channel == Other.Channel && Value == Other.Value;
	}

	FORCEINLINE bool operator<(const FMidiNote &Other) const
	{
		return Start < Other.Start ||
			(Start == Other.Start && (Channel<Other.Channel ||
				(Channel == Other.Channel && (Value<Other.Value))));
	}

};

/*
* A component that gives an actor the ability to manipulate a midi asset or to create
* their own asset
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), meta=(DisplayName = "MidiComponent") )
class MIDI_API UMidiComponent : public UActorComponent, public MidiEventListener
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMidiComponent();
	~UMidiComponent();

	// Called when the game starts
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;
	
// MIDI
	
	// loads the Midi Asset Data
	UFUNCTION(BlueprintCallable, Category = "Midi")
	void LoadAsset(class UMidiAsset* MidiAsset);
	
	UFUNCTION(BlueprintCallable, Category = "Midi")
	void LoadFile(FString path);

	UFUNCTION(BlueprintCallable, Category = "Midi")
	void ProcessTo(float time);

	UFUNCTION(BlueprintCallable, Category = "Midi")
	void GetMidiNotes(TArray<FMidiNote>& notes);

// Other
//-----------------------

private:
	MidiFile* mMidiFile;
	MidiProcessor mProcessor;

	// Midi Event Listener

	void onEvent(MidiEvent* _event);
	void onStart(bool fromBeginning);
	void onStop(bool finish);

public:
	UFUNCTION(BlueprintCallable, Category = "Midi")
	void start();
	UFUNCTION(BlueprintCallable, Category = "Midi")
	void stop();
	UFUNCTION(BlueprintCallable, Category = "Midi")
	void reset();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Midi")
	bool isStarted();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Midi")
	bool isRunning();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Midi")
	int GetResolution();

protected:
	UPROPERTY(BlueprintAssignable, Category = "Midi")
	FEventStart OnStart;
	UPROPERTY(BlueprintAssignable, Category = "Midi")
	FEventStop OnStop;
	// Called when a Note On/Off is called
	UPROPERTY(BlueprintAssignable, Category = "Midi")
	FEventMidi OnEvent;

	//called when any ChannelEvent Midi Message is called
	UPROPERTY(BlueprintAssignable, Category = "Midi")
	FEventDevice OnSend;


};
