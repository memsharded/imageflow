extern crate image;
use std::fs::File;
use std::path::Path;

use image::GenericImage;


pub fn convert(){
      // Use the open function to load an image from a Path.
    // ```open``` returns a dynamic image.
    let img = image::open(&Path::new("test.jpg")).unwrap();

    // The dimensions method returns the images width and height
    println!("dimensions {:?}", img.dimensions());

    // The color method returns the image's ColorType
    println!("{:?}", img.color());

    let ref mut fout = File::create(&Path::new("test2.jpg")).unwrap();

    // Write the contents of this image to the Writer in PNG format.
    let _ = img.save(fout, image::JPEG).unwrap();

}
pub fn hello() {
    println!("Hellow World");
}



#[cfg(test)]
mod test {

    #[test]
    fn hello_test() {
       ::hello();
    }

    #[test]
    fn test_convert(){
      ::convert();
    }
    //#[test]
    //#[should_panic]

}