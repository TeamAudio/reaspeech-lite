export interface AudioSource {
  name: string;
  persistentID: string;
  sampleRate: number;
  sampleCount: number;
  duration: number;
  channelCount: number;
  merits64BitSamples: boolean;
}

// An ARA PlaybackRegion, also known as a media item
export interface PlaybackRegion {
  playbackStart: number;
  playbackEnd: number;
  modificationStart: number;
  modificationEnd: number;
  audioSourcePersistentID: string;
}

// An ARA RegionSequence, also known as a track
export interface RegionSequence {
  name: string;
  orderIndex: number;
  playbackRegions: PlaybackRegion[];
}
