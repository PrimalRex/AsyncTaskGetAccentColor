# AsyncTaskGetAccentColor

`UAsyncTaskGetAccentColor` is an asynchronous function for extracting accent colors from a `UTexture2DDynamic`.
It uses parallel color quantization, image downsampling, and heuristic filtering to efficiently determine a vibrant but non-pallid color from a given image.

<p align="center">
  <img src="https://github.com/user-attachments/assets/f44c3fec-b847-4593-bc46-212449c8f7a7" alt="Async Accent Example" width="600">
</p>

Note: This implementation was designed to handle dynamic images where pixel data may not have been saved, there is a slow step in which there is a GPU copy to extract the pixels before performing the color determination algorithm, please feel free to bypass this step if you wish to integrate this with `UTexture2D` or other subclasses.
