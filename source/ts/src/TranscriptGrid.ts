import { AudioSource, PlaybackRegion } from './ARA';
import { Segment } from './ASR';
import { htmlEscape, timestampToString } from './Utils';

import {
  CellClickedEvent,
  ClientSideRowModelApiModule,
  ClientSideRowModelModule,
  ColDef,
  GridApi,
  GridOptions,
  ICellRendererParams,
  ModuleRegistry,
  RowStyleModule,
  TextFilterModule,
  ValidationModule,
  colorSchemeDark,
  createGrid,
  themeQuartz as theme,
} from "ag-grid-community";

ModuleRegistry.registerModules([
  ClientSideRowModelModule,
  ClientSideRowModelApiModule,
  RowStyleModule,
  TextFilterModule,
  ValidationModule,
]);

interface TranscriptRow extends Segment {
  id: string;
  playbackStart?: number;
  playbackEnd?: number;
  source: string;
  sourceID: string;
}

export default class TranscriptGrid {
  private gridElement: HTMLElement;
  private gridApi: GridApi;
  private onPlayAt: (seconds: number) => void;
  private rowData: TranscriptRow[] = [];

  constructor(selector: string, onPlayAt: (seconds: number) => void) {
    this.onPlayAt = onPlayAt;
    this.gridElement = document.querySelector(selector) as HTMLElement;
    this.gridApi = createGrid(this.gridElement, this.getGridOptions());
  }

  addRows(rows: TranscriptRow[]) {
    this.gridApi.applyTransaction({ add: rows });
    this.rowData.push(...rows);
  }

  addSegments(segments: Segment[], audioSource: AudioSource) {
    const rows: TranscriptRow[] = segments.map((segment, index) => ({
      id: audioSource.persistentID + '-' + index,
      start: segment.start,
      end: segment.end,
      playbackStart: segment.start,
      playbackEnd: segment.end,
      text: segment.text,
      score: segment.score,
      source: audioSource.name,
      sourceID: audioSource.persistentID,
    }));

    this.addRows(rows);
  }

  clear() {
    this.gridApi.applyTransaction({ remove: this.rowData });
    this.rowData.length = 0;
  }

  findPlayableRange(playbackRegions: PlaybackRegion[], segmentStart: number, segmentEnd: number) {
    for (const pr of playbackRegions) {
      const playbackStart = pr.playbackStart;
      const playbackEnd = pr.playbackEnd;
      const modificationStart = pr.modificationStart;

      const start = playbackStart + segmentStart - modificationStart;
      const end = playbackStart + segmentEnd - modificationStart;

      if (start >= playbackStart && start <= playbackEnd) {
        return { start, end };
      }
    }

    return null;
  }

  getColumnDefs(): ColDef<TranscriptRow>[] {
    return [
      {
        field: 'id',
        headerName: 'ID',
        hide: true
      },
      {
        field: 'playbackStart',
        headerName: 'Start',
        cellRenderer: this.startTimeCellRenderer,
        width: 100
      },
      {
        field: 'playbackEnd',
        headerName: 'End',
        cellRenderer: this.endTimeCellRenderer,
        width: 100
      },
      {
        field: 'text',
        headerName: 'Text',
        filter: true,
        cellRenderer: this.textCellRenderer,
        flex: 1
      },
      {
        field: 'score',
        headerName: 'Score',
        cellRenderer: this.scoreCellRenderer,
        width: 100
      },
      {
        field: 'source',
        headerName: 'Source',
        filter: true,
        width: 200
      },
    ];
  }

  getGridOptions(): GridOptions<TranscriptRow> {
    return {
      columnDefs: this.getColumnDefs(),
      getRowId: this.getRowId,
      onCellClicked: this.handleCellClicked.bind(this),
      rowData: this.rowData,
      rowHeight: 32,

      theme: theme.withPart(colorSchemeDark).withParams({
        borderColor: '#4C545B',
        headerBackgroundColor: '#212529',
      }),

      getRowStyle: (params) => {
        if (params.node.rowIndex % 2 === 0) {
          return { background: '#2C3035' }
        } else {
          return { background: '#212529' }
        }
      },
    };
  }

  getRowId(params: { data: TranscriptRow }) {
    return params.data.id;
  }

  getRows(): TranscriptRow[] {
    return this.rowData;
  }

  handleCellClicked(params: CellClickedEvent) {
    if (params.column.getColId() === 'playbackStart' || params.column.getColId() === 'text') {
      const target = params.event.target as HTMLElement;
      if (target.tagName === 'A' && params.data.playbackStart !== null) {
        this.onPlayAt(params.data.playbackStart);
      }
    }
  }

  scoreColor(value: number): string {
    if (value > 0.9) {
      return "#a3ff00";
    } else if (value > 0.8) {
      return "#2cba00";
    } else if (value > 0.7) {
      return "#ffa700";
    } else if (value > 0.0) {
      return "#ff2c2f";
    } else {
      return "transparent";
    }
  }

  setPlaybackRegionMap(playbackRegionsBySourceID: Map<string, PlaybackRegion[]>) {
    const updatedRows = this.rowData.map(row => {
      const playbackRegions = playbackRegionsBySourceID.get(row.sourceID);

      if (playbackRegions) {
        const range = this.findPlayableRange(playbackRegions, row.start, row.end);
        const newStart = range?.start ?? null;
        const newEnd = range?.end ?? null;

        if (row.playbackStart !== newStart || row.playbackEnd !== newEnd) {
          row.playbackStart = newStart;
          row.playbackEnd = newEnd;
          return row;
        }
      } else if (row.playbackStart !== null || row.playbackEnd !== null) {
        row.playbackStart = null;
        row.playbackEnd = null;
        return row;
      }

      return null;
    }).filter(row => row !== null);

    if (updatedRows.length > 0) {
      this.updateRows(updatedRows);
    }
  }

  updateRows(rows: TranscriptRow[]) {
    this.gridApi.applyTransaction({ update: rows });
  }

  // Define cell renderer methods as instance properties (arrow functions to preserve "this" context)
  startTimeCellRenderer = (params: ICellRendererParams) => {
    const linkClasses = 'link-offset-2 link-underline link-underline-opacity-0 link-underline-opacity-50-hover small';
    const time = params.value;
    if (time === null) {
      return '';
    }
    return `<a href="javascript:" class="${linkClasses}" data-segment-time="${time}">${timestampToString(time)}</a>`;
  }

  endTimeCellRenderer = (params: ICellRendererParams) => {
    const time = params.value;
    if (time === null) {
      return '';
    }
    return `<span class="small text-muted" data-segment-time="${time}">${timestampToString(time)}</span>`;
  }

  textCellRenderer = (params: ICellRendererParams) => {
    const linkClasses = 'link-light link-offset-2 link-underline link-underline-opacity-0 link-underline-opacity-50-hover';
    return `<a href="javascript:" class="${linkClasses}">${htmlEscape(params.value)}</a>`;
  }

  scoreCellRenderer = (params: ICellRendererParams) => {
    const score = params.value;
    const color = this.scoreColor(score);
    const percentage = score * 100;
    return `<div class="d-flex align-items-center h-100">
              <div class="progress w-100" style="height: 2px">
                <div class="progress-bar" style="width: ${percentage}%; background-color: ${color}"></div>
              </div>
            </div>`;
  }
}
