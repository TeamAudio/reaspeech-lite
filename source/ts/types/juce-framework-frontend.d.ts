/**
 * Type definitions for juce-framework-frontend
 */

declare module 'juce-framework-frontend' {
  /**
   * Gets a native function by name
   * @param name - The name of the native function to get
   * @returns The native function
   */
  export const getNativeFunction: (name: string) => Function;
}
