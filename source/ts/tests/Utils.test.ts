import { describe, expect, it } from '@jest/globals';
import { htmlEscape, timestampToString } from '../src/Utils';

describe('Utils', () => {
  describe('htmlEscape', () => {
    it('should escape HTML special characters', () => {
      expect(htmlEscape('&<>"')).toBe('&amp;&lt;&gt;&quot;');
    });

    it('should return the same string when no special characters are present', () => {
      expect(htmlEscape('Hello World')).toBe('Hello World');
    });

    it('should handle empty strings', () => {
      expect(htmlEscape('')).toBe('');
    });

    it('should handle strings with multiple occurrences of special characters', () => {
      expect(htmlEscape('<div>Hello & "World"</div>')).toBe('&lt;div&gt;Hello &amp; &quot;World&quot;&lt;/div&gt;');
    });
  });

  describe('timestampToString', () => {
    it('should format seconds into M:SS.mmm', () => {
      expect(timestampToString(65.123)).toBe('1:05.123');
    });

    it('should handle zero seconds', () => {
      expect(timestampToString(0)).toBe('0:00.000');
    });

    it('should handle seconds less than a minute', () => {
      expect(timestampToString(45.5)).toBe('0:45.500');
    });

    it('should floor milliseconds', () => {
      expect(timestampToString(10.1234)).toBe('0:10.123');
      expect(timestampToString(10.1239)).toBe('0:10.123');
    });

    it('should pad seconds and milliseconds with leading zeros', () => {
      expect(timestampToString(1.1)).toBe('0:01.100');
      expect(timestampToString(10.01)).toBe('0:10.010');
    });

    it('should handle large values', () => {
      expect(timestampToString(3661.789)).toBe('1:01:01.789');
    });

    it('should handle negative values', () => {
      expect(timestampToString(-65.123)).toBe('-1:05.123');
      expect(timestampToString(-3661.789)).toBe('-1:01:01.789');
    });
  });
});
