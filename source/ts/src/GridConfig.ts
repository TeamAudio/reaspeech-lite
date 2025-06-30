import {
  ClientSideRowModelApiModule,
  ClientSideRowModelModule,
  CsvExportModule,
  ModuleRegistry,
  QuickFilterModule,
  RowApiModule,
  RowSelectionModule,
  RowStyleModule,
  ScrollApiModule,
  TextFilterModule,
  ValidationModule,
  colorSchemeDark,
  themeQuartz as theme,
} from "ag-grid-community";

ModuleRegistry.registerModules([
  ClientSideRowModelApiModule,
  ClientSideRowModelModule,
  CsvExportModule,
  QuickFilterModule,
  RowApiModule,
  RowSelectionModule,
  RowStyleModule,
  ScrollApiModule,
  TextFilterModule,
  ValidationModule,
]);

export function getTheme() {
  return theme.withPart(colorSchemeDark).withParams({
    backgroundColor: '#2C3035',
    borderColor: '#4C545B',
    headerBackgroundColor: '#212529',
  })
}

export const stripedRowStyle = (params) => {
  if (params.node.rowIndex % 2 === 0) {
    return { background: '#2C3035' }
  } else {
    return { background: '#212529' }
  }
}
