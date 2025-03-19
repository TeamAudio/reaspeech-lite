import TranscriptGrid from '../src/TranscriptGrid';
import { AudioSource, PlaybackRegion } from '../src/ARA';
import { beforeEach, describe, expect, it, jest } from '@jest/globals';

// Mock ag-grid modules
jest.mock('ag-grid-community', () => {
  const original = jest.requireActual('ag-grid-community') as object;
  return {
    ...original,
    ModuleRegistry: {
      registerModules: jest.fn(),
    },
    createGrid: jest.fn(() => ({
      applyTransaction: jest.fn(),
    })),
  };
});

function makeAudioSource(name: string, persistentID: string): AudioSource {
  return {
    name: name,
    persistentID: persistentID,
    sampleRate: 44100,
    sampleCount: 441000,
    duration: 10,
    channelCount: 2,
    merits64BitSamples: false
  };
}

describe('TranscriptGrid', () => {
  let grid: TranscriptGrid;
  let mockElement: HTMLElement;
  let onPlayAt: jest.Mock;

  beforeEach(() => {
    mockElement = document.createElement('div');
    document.querySelector = jest.fn().mockReturnValue(mockElement);
    onPlayAt = jest.fn();
    grid = new TranscriptGrid('#grid', onPlayAt);
  });

  it('should initialize with correct selector and callback', () => {
    expect(document.querySelector).toHaveBeenCalledWith('#grid');
    expect(grid['onPlayAt']).toBe(onPlayAt);
  });

  it('should create row data with correct format when adding rows', () => {
    const segments = [
      { start: 10, end: 15, text: 'Hello', score: 0.95 },
      { start: 16, end: 20, text: 'World', score: 0.85 }
    ];
    const audioSource = makeAudioSource('Test Audio', 'test123');

    grid.addSegments(segments, audioSource);

    expect(grid['gridApi'].applyTransaction).toHaveBeenCalledWith({
      add: [
        {
          id: 'test123-0',
          start: 10,
          end: 15,
          playbackStart: 10,
          playbackEnd: 15,
          text: 'Hello',
          score: 0.95,
          source: 'Test Audio',
          sourceID: 'test123'
        },
        {
          id: 'test123-1',
          start: 16,
          end: 20,
          playbackStart: 16,
          playbackEnd: 20,
          text: 'World',
          score: 0.85,
          source: 'Test Audio',
          sourceID: 'test123'
        }
      ]
    });
  });

  it('should clear grid when calling clear', () => {
    const segments = [
      { start: 10, end: 15, text: 'Hello', score: 0.95 },
      { start: 16, end: 20, text: 'World', score: 0.85 }
    ];
    const audioSource = makeAudioSource('Test Audio', 'test123');

    grid.addSegments(segments, audioSource);

    const rows = grid.getRows();
    expect(rows).toHaveLength(2);
    (grid['gridApi'].applyTransaction as jest.Mock).mockClear();

    grid.clear();

    expect(grid['gridApi'].applyTransaction).toHaveBeenCalledWith({ remove: rows });
    expect(grid.getRows()).toHaveLength(0);
  });

  it('should find playable range based on playback regions', () => {
    const playbackRegions = [
      {
        playbackStart: 10,
        playbackEnd: 20,
        modificationStart: 0,
        modificationEnd: 10,
        audioSourcePersistentID: 'test123'
      },
      {
        playbackStart: 30,
        playbackEnd: 40,
        modificationStart: 20,
        modificationEnd: 30,
        audioSourcePersistentID: 'test456'
      }
    ];

    const range = grid.findPlayableRange(playbackRegions, 5, 10);
    expect(range).toEqual({ start: 15, end: 20 });
  });

  it('should return null if no playable range is found', () => {
    const playbackRegions = [
      {
        playbackStart: 10,
        playbackEnd: 20,
        modificationStart: 0,
        modificationEnd: 10,
        audioSourcePersistentID: 'test123'
      }
    ];

    const range = grid.findPlayableRange(playbackRegions, 30, 40);
    expect(range).toBeNull();
  });

  it('should return correct column definitions', () => {
    const columnDefs = grid.getColumnDefs();

    expect(columnDefs).toHaveLength(6);
    expect(columnDefs[0].field).toBe('id');
    expect(columnDefs[1].field).toBe('playbackStart');
    expect(columnDefs[2].field).toBe('playbackEnd');
    expect(columnDefs[3].field).toBe('text');
    expect(columnDefs[4].field).toBe('score');
    expect(columnDefs[5].field).toBe('source');
  });

  it('should generate correct grid options', () => {
    const options = grid.getGridOptions();

    expect(options.columnDefs).toEqual(grid.getColumnDefs());
    expect(options.rowData).toEqual([]);
    expect(typeof options.getRowId).toBe('function');
    expect(typeof options.onCellClicked).toBe('function');

    // Test that rows with even/odd indexes get the same styling
    const rowStyles = Array.from({ length: 4 }, (_, i) => {
      const mockParams = { node: { rowIndex: i } } as any;
      return options.getRowStyle!(mockParams);
    });

    expect(rowStyles[0]).toEqual(rowStyles[2]); // Even rows match
    expect(rowStyles[1]).toEqual(rowStyles[3]); // Odd rows match
  });

  it('should return correct row ID based on source and index', () => {
    expect(grid.getRowId({
      data: {
        id: 'test123-0',
        source: 'Test Audio',
        sourceID: 'test123',
        start: 0,
        end: 0,
        playbackStart: 0,
        playbackEnd: 0,
        text: '',
        score: 0
      }
    })).toBe('test123-0');
  });

  it('should return correct colors based on score', () => {
    expect(grid.scoreColor(0.95)).toBe('#a3ff00');
    expect(grid.scoreColor(0.85)).toBe('#2cba00');
    expect(grid.scoreColor(0.75)).toBe('#ffa700');
    expect(grid.scoreColor(0.65)).toBe('#ff2c2f');
    expect(grid.scoreColor(0)).toBe('transparent');
  });

  it('should update rows with correct playback regions', () => {
    const playbackRegionsBySourceID = new Map<string, PlaybackRegion[]>();
    playbackRegionsBySourceID.set('test123', [
      {
        playbackStart: 10,
        playbackEnd: 20,
        modificationStart: 0,
        modificationEnd: 10,
        audioSourcePersistentID: 'test123'
      }
    ]);

    grid.addSegments([{ start: 0, end: 10, text: '', score: 0 }], makeAudioSource('Test Audio', 'test123'));
    grid.setPlaybackRegionMap(playbackRegionsBySourceID);

    const rows = grid.getRows();
    expect(rows).toHaveLength(1);
    expect(rows[0].playbackStart).toBe(10);
    expect(rows[0].playbackEnd).toBe(20);

    expect(grid['gridApi'].applyTransaction).toHaveBeenCalledWith({
      update: [rows[0]]
    });
  });

  it('should set playbackStart and playbackEnd to null when no matching playback region exists', () => {
    const playbackRegionsBySourceID = new Map<string, PlaybackRegion[]>();

    grid.addSegments([{ start: 0, end: 10, text: '', score: 0 }], makeAudioSource('Test Audio', 'test123'));
    grid.setPlaybackRegionMap(playbackRegionsBySourceID);

    const rows = grid.getRows();
    expect(rows).toHaveLength(1);
    expect(rows[0].playbackStart).toBeNull();
    expect(rows[0].playbackEnd).toBeNull();
  });

  it('should set playbackStart and playbackEnd to null when segment is outside any playback region', () => {
    const playbackRegionsBySourceID = new Map<string, PlaybackRegion[]>();
    playbackRegionsBySourceID.set('test123', [
      {
        playbackStart: 50,
        playbackEnd: 60,
        modificationStart: 30,
        modificationEnd: 40,
        audioSourcePersistentID: 'test123'
      }
    ]);

    // Add a segment with start/end outside the modification range
    grid.addSegments([{ start: 0, end: 10, text: '', score: 0 }], makeAudioSource('Test Audio', 'test123'));

    expect(grid.getRows()[0].playbackStart).not.toBeNull();
    expect(grid.getRows()[0].playbackEnd).not.toBeNull();

    grid.setPlaybackRegionMap(playbackRegionsBySourceID);

    expect(grid.getRows()[0].playbackStart).toBeNull();
    expect(grid.getRows()[0].playbackEnd).toBeNull();
  });

  it('should not update rows when playback regions have not changed', () => {
    const playbackRegionsBySourceID = new Map<string, PlaybackRegion[]>();
    playbackRegionsBySourceID.set('test123', [
      {
        playbackStart: 10,
        playbackEnd: 20,
        modificationStart: 0,
        modificationEnd: 10,
        audioSourcePersistentID: 'test123'
      }
    ]);

    grid.addSegments([{ start: 0, end: 10, text: '', score: 0 }], makeAudioSource('Test Audio', 'test123'));

    // First call should update
    grid.setPlaybackRegionMap(playbackRegionsBySourceID);

    // Clear the mock to check if it's called again
    (grid['gridApi'].applyTransaction as jest.Mock).mockClear();

    // Second call with the same data should not update
    grid.setPlaybackRegionMap(playbackRegionsBySourceID);

    expect(grid['gridApi'].applyTransaction).not.toHaveBeenCalled();
  });

  it('should render empty string for start time when value is null', () => {
    const params = { value: null } as any;
    const result = grid.startTimeCellRenderer(params);
    expect(result).toBe('');
  });

  it('should render empty string for end time when value is null', () => {
    const params = { value: null } as any;
    const result = grid.endTimeCellRenderer(params);
    expect(result).toBe('');
  });

  it('should render start time with link', () => {
    const params = { value: 10 } as any;
    const result = grid.startTimeCellRenderer(params);

    expect(result).toContain('0:10.000');
    expect(result).toContain('href="javascript:"');
    expect(result).toContain('data-segment-time="10"');
  });

  it('should render end time', () => {
    const params = { value: 10 } as any;
    const result = grid.endTimeCellRenderer(params);

    expect(result).toContain('0:10.000');
    expect(result).toContain('data-segment-time="10"');
  });

  it('should render text with link', () => {
    const params = { value: 'Test text' } as any;
    const result = grid.textCellRenderer(params);

    expect(result).toContain('Test text');
    expect(result).toContain('href="javascript:"');
  });

  it('should render score bar with correct width and color', () => {
    const params = { value: 0.75 } as any;
    const result = grid.scoreCellRenderer(params);

    expect(result).toContain('width: 75%');
    expect(result).toContain(`background-color: ${grid.scoreColor(0.75)}`);
  });

  it('should call onPlayAt when link in column is clicked', () => {
    const options = grid.getGridOptions();
    const params = {
      column: { getColId: () => 'playbackStart' },
      data: { start: 15, playbackStart: 15 },
      event: { target: document.createElement('a') }
    } as any;

    options.onCellClicked!(params);
    expect(onPlayAt).toHaveBeenCalledWith(15);
  });

  it('should not call onPlayAt when non-link portion of column is clicked', () => {
    const options = grid.getGridOptions();
    const params = {
      column: { getColId: () => 'start' },
      data: { start: 15 },
      event: { target: document.createElement('div') }
    } as any;

    options.onCellClicked!(params);
    expect(onPlayAt).not.toHaveBeenCalled();
  });
});
