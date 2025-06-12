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

  it('should remove rows by audio source ID', () => {
    const segments1 = [
      { start: 10, end: 15, text: 'Hello', score: 0.95 },
      { start: 16, end: 20, text: 'World', score: 0.85 }
    ];
    const segments2 = [
      { start: 5, end: 8, text: 'Different', score: 0.90 },
      { start: 25, end: 30, text: 'Source', score: 0.80 }
    ];
    const audioSource1 = makeAudioSource('Test Audio 1', 'test123');
    const audioSource2 = makeAudioSource('Test Audio 2', 'test456');

    grid.addSegments(segments1, audioSource1);
    grid.addSegments(segments2, audioSource2);

    const allRows = grid.getRows();
    expect(allRows).toHaveLength(4);

    const rowsToRemove = allRows.filter(row => row.sourceID === 'test123');
    const rowsToKeep = allRows.filter(row => row.sourceID === 'test456');
    expect(rowsToRemove).toHaveLength(2);
    expect(rowsToKeep).toHaveLength(2);

    (grid['gridApi'].applyTransaction as jest.Mock).mockClear();

    grid.removeRowsBySourceID('test123');

    expect(grid['gridApi'].applyTransaction).toHaveBeenCalledWith({ remove: rowsToRemove });
    expect(grid.getRows()).toHaveLength(2);

    // Verify remaining rows are from the other source
    const remainingRows = grid.getRows();
    expect(remainingRows.every(row => row.sourceID === 'test456')).toBe(true);
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

    expect(options.columnDefs).toBeDefined();
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

  it('should deselect all rows when not playing', () => {
    grid['gridApi'].deselectAll = jest.fn();
    grid.setPlaybackPosition(5, false);
    expect(grid['gridApi'].deselectAll).toHaveBeenCalled();
  });

  it('should select and scroll to row containing playback position when playing', () => {
    grid.addSegments([
      { start: 0, end: 10, text: 'Segment 1', score: 0.9 },
      { start: 10, end: 20, text: 'Segment 2', score: 0.8 },
      { start: 20, end: 30, text: 'Segment 3', score: 0.7 }
    ], makeAudioSource('Test Audio', 'test123'));

    grid['gridApi'].getRowNode = jest.fn().mockReturnValue({
      setSelected: jest.fn(),
      isSelected: jest.fn().mockReturnValue(false)
    }) as jest.Mock<any>;

    grid['gridApi'].deselectAll = jest.fn();
    grid['gridApi'].ensureNodeVisible = jest.fn();

    grid.setPlaybackPosition(15, true);

    expect(grid['gridApi'].getRowNode).toHaveBeenCalledWith('test123-1');
    const node = grid['gridApi'].getRowNode('test123-1');
    expect(node?.setSelected).toHaveBeenCalledWith(true);
    expect(grid['gridApi'].deselectAll).toHaveBeenCalled();
    expect(grid['gridApi'].ensureNodeVisible).toHaveBeenCalledWith(node);
  });

  it('should not change selection if position is outside any segment', () => {
    grid.addSegments([
      { start: 0, end: 10, text: 'Segment 1', score: 0.9 },
      { start: 10, end: 20, text: 'Segment 2', score: 0.8 },
      { start: 20, end: 30, text: 'Segment 3', score: 0.7 }
    ], makeAudioSource('Test Audio', 'test123'));

    grid['gridApi'].getRowNode = jest.fn().mockReturnValue({
      setSelected: jest.fn(),
      isSelected: jest.fn().mockReturnValue(false)
    }) as jest.Mock<any>;

    grid['gridApi'].deselectAll = jest.fn();
    grid['gridApi'].ensureNodeVisible = jest.fn();

    grid.setPlaybackPosition(35, true);

    expect(grid['gridApi'].getRowNode).not.toHaveBeenCalled();
    expect(grid['gridApi'].deselectAll).not.toHaveBeenCalled();
    expect(grid['gridApi'].ensureNodeVisible).not.toHaveBeenCalled();
  });

  it('should not select row if already selected', () => {
    grid.addSegments([
      { start: 0, end: 10, text: 'Segment 1', score: 0.9 },
      { start: 10, end: 20, text: 'Segment 2', score: 0.8 },
      { start: 20, end: 30, text: 'Segment 3', score: 0.7 }
    ], makeAudioSource('Test Audio', 'test123'));

    grid['gridApi'].getRowNode = jest.fn().mockReturnValue({
      setSelected: jest.fn(),
      isSelected: jest.fn().mockReturnValue(true)
    }) as jest.Mock<any>;

    grid['gridApi'].deselectAll = jest.fn();
    grid['gridApi'].ensureNodeVisible = jest.fn();

    grid.setPlaybackPosition(15, true);

    expect(grid['gridApi'].getRowNode).toHaveBeenCalledWith('test123-1');
    const node = grid['gridApi'].getRowNode('test123-1');
    expect(node?.setSelected).not.toHaveBeenCalled();
    expect(grid['gridApi'].deselectAll).not.toHaveBeenCalled();
    expect(grid['gridApi'].ensureNodeVisible).not.toHaveBeenCalled();
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
    const result = grid.renderStartTime(params);
    expect(result).toBe('');
  });

  it('should render empty string for end time when value is null', () => {
    const params = { value: null } as any;
    const result = grid.renderEndTime(params);
    expect(result).toBe('');
  });

  it('should render start time with link', () => {
    const params = { value: 10 } as any;
    const result = grid.renderStartTime(params);

    expect(result).toContain('0:10.000');
    expect(result).toContain('href="javascript:"');
    expect(result).toContain('data-segment-time="10"');
  });

  it('should render end time', () => {
    const params = { value: 10 } as any;
    const result = grid.renderEndTime(params);

    expect(result).toContain('0:10.000');
    expect(result).toContain('data-segment-time="10"');
  });

  it('should render text with link', () => {
    const params = { value: 'Test text' } as any;
    const result = grid.renderText(params);

    expect(result).toContain('Test text');
    expect(result).toContain('href="javascript:"');
  });

  it('should render score bar with correct width and color', () => {
    const params = { value: 0.75 } as any;
    const result = grid.renderScore(params);

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

  it('exports CSV', () => {
    const row1 = {
      id: 'test123-0',
      start: 10,
      end: 20,
      text: 'Hello',
      score: 0.95,
      source: 'Test Audio',
      sourceID: 'test123'
    };
    const row2 = {
      id: 'test123-1',
      start: 30,
      end: 40,
      text: 'World',
      score: 0.85,
      source: 'Test Audio',
      sourceID: 'test123'
    };
    grid.addRows([row1, row2]);

    grid['gridApi'].getDataAsCsv = jest.fn((params) => {
      return 'generated CSV content';
    });

    let content = '', mimeType = '', filename = '';
    grid.exportCSV((c, m, f) => {
      content = c;
      mimeType = m;
      filename = f;
    });

    expect(content).toBe('generated CSV content');
    expect(mimeType).toBe('text/csv;charset=utf-8');
    expect(filename).toBe('transcript.csv');
  });

  it('exports SRT', () => {
    const row1 = {
      id: 'test123-0',
      start: 10,
      end: 20,
      playbackStart: 10,
      playbackEnd: 20,
      text: 'Hello',
      score: 0.95,
      source: 'Test Audio',
      sourceID: 'test123'
    };
    const row2 = {
      id: 'test123-1',
      start: 30,
      end: 40,
      playbackStart: 30,
      playbackEnd: 40,
      text: 'World',
      score: 0.85,
      source: 'Test Audio',
      sourceID: 'test123'
    };
    grid.addRows([row1, row2]);

    let content = '', mimeType = '', filename = '';
    grid.exportSRT((c, m, f) => {
      content = c;
      mimeType = m;
      filename = f;
    });

    expect(content).toBe('1\n00:00:10,000 --> 00:00:20,000\nHello\n\n2\n00:00:30,000 --> 00:00:40,000\nWorld\n');
    expect(mimeType).toBe('application/x-subrip');
    expect(filename).toBe('transcript.srt');
  });

  it('processes cells for CSV export', () => {
    // Test timestamp formatting
    const timeParams = {
      value: 10,
      column: { getColId: () => 'playbackStart' },
    } as any;
    expect(grid.processCellForCSV(timeParams)).toBe('0:10.000');

    // Test null values
    timeParams.value = null;
    expect(grid.processCellForCSV(timeParams)).toBe('');

    // Test score formatting
    const scoreParams = {
      value: 0.756,
      column: { getColId: () => 'score' },
    } as any;
    expect(grid.processCellForCSV(scoreParams)).toBe('0.76');

    // Test null score
    scoreParams.value = null;
    expect(grid.processCellForCSV(scoreParams)).toBe('');

    // Test other columns
    const otherParams = {
      value: 'test',
      column: { getColId: () => 'text' },
    } as any;
    expect(grid.processCellForCSV(otherParams)).toBe('test');
  });

  it('processes rows for SRT export', () => {
    const row = {
      id: 'test123-0',
      start: 10,
      end: 20,
      playbackStart: 10,
      playbackEnd: 20,
      text: 'Hello',
      score: 0.95,
      source: 'Test Audio',
      sourceID: 'test123',
    };
    const index = 0;
    const result = grid.processRowForSRT(row, index);
    expect(result).toBe('1\n00:00:10,000 --> 00:00:20,000\nHello\n');
  });
});
