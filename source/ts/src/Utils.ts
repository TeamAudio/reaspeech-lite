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
