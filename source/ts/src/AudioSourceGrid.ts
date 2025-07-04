import * as GridConfig from './GridConfig';
import { AudioSource } from './ARA';
import { timestampToString } from './Utils';

import {
  ColDef,
  GridApi,
  GridOptions,
  ICellRendererParams,
  createGrid,
} from "ag-grid-community";

interface AudioSourceRow extends AudioSource {
}

export default class AudioSourceGrid {
  private gridElement: HTMLElement;
  private gridApi: GridApi;
  private rowData: AudioSourceRow[] = [];

  constructor(selector: string) {
    this.gridElement = document.querySelector(selector) as HTMLElement;
    this.gridApi = createGrid(this.gridElement, this.getGridOptions());
  }

  addRows(rows: AudioSourceRow[]) {
    this.gridApi.applyTransaction({ add: rows });
    this.rowData.push(...rows);
  }

  clear() {
    this.gridApi.applyTransaction({ remove: this.rowData });
    this.rowData.length = 0;
  }

  getColumnDefs(): ColDef<AudioSourceRow>[] {
    return [
      {
        field: 'persistentID',
        headerName: 'ID',
        hide: true
      },
      {
        field: 'name',
        headerName: 'Name',
        flex: 1
      },
      {
        field: 'duration',
        headerName: 'Duration',
        cellRenderer: this.renderDuration.bind(this),
        width: 100
      },
    ];
  }

  getGridOptions(): GridOptions<AudioSourceRow> {
    return {
      columnDefs: this.getColumnDefs(),
      getRowId: this.getRowId,
      overlayNoRowsTemplate: 'No audio sources found',
      rowData: this.rowData,
      rowHeight: 32,
      rowSelection: { mode: 'multiRow', checkboxes: true, headerCheckbox: true },
      suppressCellFocus: true,
      suppressDragLeaveHidesColumns: true,
      theme: GridConfig.getTheme(),
    };
  }

  getRowId(params: { data: AudioSourceRow }) {
    return params.data.persistentID;
  }

  getRows(): AudioSourceRow[] {
    return this.rowData;
  }

  getSelectedRows(): AudioSourceRow[] {
    return this.gridApi.getSelectedRows();
  }

  getSelectedRowIds(): string[] {
    return this.gridApi.getSelectedRows().map(row => row.persistentID);
  }

  selectAll(): void {
    this.gridApi.selectAll();
  }

  setRowSelected(id: string, selected: boolean): void {
    this.gridApi.forEachNode(node => {
      if (node.data && node.data.persistentID === id) {
        node.setSelected(selected, false);
      }
    });
  }

  setSelectedRowIds(rowIds: string[]): void {
    this.gridApi.forEachNode(node => {
      if (node.data && rowIds.includes(node.data.persistentID)) {
        node.setSelected(true, false);
      } else {
        node.setSelected(false, false);
      }
    });
  }

  renderDuration(params: ICellRendererParams) {
    return timestampToString(params.value);
  }
}
