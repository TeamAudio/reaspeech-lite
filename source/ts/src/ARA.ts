export interface AudioSource {
  name: string;
  persistentID: string;
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
  start: number;
  end: number;
  playbackRegions: PlaybackRegion[];
}
