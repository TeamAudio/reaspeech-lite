/**
 * Escapes HTML special characters in a string.
 * @param str The string to escape.
 * @returns The escaped string.
 */
export function htmlEscape(str: string): string {
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

/**
 * Returns a string of the form M:SS.mmm
 * @param seconds The time in seconds.
 * @returns The formatted string.
 */
export function timestampToString(seconds: number): string {
  const minutes = Math.floor(seconds / 60);
  const remainingSeconds = seconds - minutes * 60;
  const milliseconds = Math.round(remainingSeconds * 1000) % 1000;
  return `${minutes}:${String(Math.floor(remainingSeconds)).padStart(2, '0')}.${String(milliseconds).padStart(3, '0')}`;
}
