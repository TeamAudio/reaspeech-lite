import AudioSourceGrid from '../src/AudioSourceGrid';
import { AudioSource } from '../src/ARA';
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

const audioSources = [
  makeAudioSource('Test Audio 1', 'audio1'),
  makeAudioSource('Test Audio 2', 'audio2')
];

describe('AudioSourceGrid', () => {
  let grid: AudioSourceGrid;
  let mockElement: HTMLElement;

  beforeEach(() => {
    mockElement = document.createElement('div');
    document.querySelector = jest.fn().mockReturnValue(mockElement);
    grid = new AudioSourceGrid('#grid');
  });

  it('should initialize with correct selector and callback', () => {
    expect(document.querySelector).toHaveBeenCalledWith('#grid');
  });

  it('should create row data with correct format when adding rows', () => {
    grid.addRows(audioSources);

    expect(grid['gridApi'].applyTransaction).toHaveBeenCalledWith({
      add: [
        {
          "channelCount": 2,
          "duration": 10,
          "merits64BitSamples": false,
          "name": "Test Audio 1",
          "persistentID": "audio1",
          "sampleCount": 441000,
          "sampleRate": 44100,
        },
        {
          "channelCount": 2,
          "duration": 10,
          "merits64BitSamples": false,
          "name": "Test Audio 2",
          "persistentID": "audio2",
          "sampleCount": 441000,
          "sampleRate": 44100,
        }
      ]
    });
  });

  it('should clear grid when calling clear', () => {
    grid.addRows(audioSources);

    const rows = grid.getRows();
    expect(rows).toHaveLength(2);
    (grid['gridApi'].applyTransaction as jest.Mock).mockClear();

    grid.clear();

    expect(grid['gridApi'].applyTransaction).toHaveBeenCalledWith({ remove: rows });
    expect(grid.getRows()).toHaveLength(0);
  });

  it('should return correct column definitions', () => {
    const columnDefs = grid.getColumnDefs();

    expect(columnDefs).toHaveLength(3);
    expect(columnDefs[0].field).toBe('persistentID');
    expect(columnDefs[1].field).toBe('name');
    expect(columnDefs[2].field).toBe('duration');
  });

  it('should generate correct grid options', () => {
    const options = grid.getGridOptions();

    expect(options.columnDefs).toBeDefined();
    expect(options.rowData).toEqual([]);
    expect(typeof options.getRowId).toBe('function');
  });

  it('should return correct row ID', () => {
    expect(grid.getRowId({
      data: makeAudioSource('Test Audio 1', 'audio1')
    })).toBe('audio1');
  });

  it('should return selected rows', () => {
    grid.addRows(audioSources);
    grid['gridApi'].getSelectedRows = jest.fn().mockReturnValue([audioSources[1]]) as jest.MockedFunction<() => AudioSource[]>;

    const selectedRows = grid.getSelectedRows();

    expect(selectedRows).toHaveLength(1);
    expect(selectedRows[0].persistentID).toEqual('audio2');
  });

  it('should return selected row IDs', () => {
    grid.addRows(audioSources);
    grid['gridApi'].getSelectedRows = jest.fn().mockReturnValue([audioSources[1]]) as jest.MockedFunction<() => AudioSource[]>;

    const selectedRowIds = grid.getSelectedRowIds();

    expect(selectedRowIds).toHaveLength(1);
    expect(selectedRowIds).toEqual(['audio2']);
  });

  it('should select all rows', () => {
    grid['gridApi'].selectAll = jest.fn();
    grid.selectAll();
    expect(grid['gridApi'].selectAll).toHaveBeenCalled();
  });

  it('should render duration', () => {
    const params = { value: 10 } as any;
    const result = grid.renderDuration(params);
    expect(result).toContain('0:10.000');
  });
});
