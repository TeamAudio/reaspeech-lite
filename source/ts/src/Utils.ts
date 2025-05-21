/**
 * Delays execution for the specified number of milliseconds.
 * @param ms The delay time in milliseconds.
 * @returns A promise that resolves after the specified delay.
 */
export function delay(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * Triggers the browser's file download behavior for a file with the specified
 * content, MIME type, and filename.
 * @param content The content of the file.
 * @param mimeType The MIME type of the file.
 * @param filename The name of the file to be downloaded.
 * @returns {void}
 */
export function downloadFile(content: string, mimeType: string, filename: string): void {
  const blob = new Blob([content], { type: mimeType });
  const url = window.URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.setAttribute('href', url);
  link.setAttribute('download', filename);
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  window.URL.revokeObjectURL(url);
}

/**
 * Escapes HTML special characters in a string.
 * @param str The string to escape.
 * @returns The escaped string.
 */
export function htmlEscape(str: string): string {
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

/**
 * Returns a string of the form H:MM:SS.mmm or M:SS.mmm from a number of seconds.
 * Milliseconds are always rounded down to three decimal places.
 * @param seconds The time in seconds.
 * @returns The formatted string.
 */
export function timestampToString(seconds: number): string {
  const neg = seconds < 0 ? '-' : '';
  seconds = Math.abs(seconds);

  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor(seconds / 60) % 60;
  const wholeSeconds = Math.floor(seconds) % 60;
  const milliseconds = Math.floor(seconds * 1000) % 1000;

  if (hours > 0) {
    return `${neg}${hours}:${String(minutes).padStart(2, '0')}:${String(wholeSeconds).padStart(2, '0')}.${String(milliseconds).padStart(3, '0')}`;
  } else {
    return `${neg}${minutes}:${String(wholeSeconds).padStart(2, '0')}.${String(milliseconds).padStart(3, '0')}`;
  }
}

/**
 * Returns a string of the form HH:MM:SS,mmm from a number of seconds.
 * This format should be compatible with SRT files.
 * Milliseconds are always rounded down to three decimal places.
 * @param seconds The time in seconds.
 * @returns The formatted string.
 */
export function timestampToStringSRT(seconds: number): string {
  const neg = seconds < 0 ? '-' : '';
  seconds = Math.abs(seconds);

  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor(seconds / 60) % 60;
  const wholeSeconds = Math.floor(seconds) % 60;
  const milliseconds = Math.floor(seconds * 1000) % 1000;

  return `${neg}${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(wholeSeconds).padStart(2, '0')},${String(milliseconds).padStart(3, '0')}`;
}
