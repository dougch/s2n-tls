use std::fs::File;
use std::io::BufReader;
use std::env;
use xml::reader::{EventReader, XmlEvent};

fn main() {
    let args: Vec<String> = env::args().collect();
    dbg!(&args);
    let input: String = args[1].clone(); 
    let file = File::open(&input)
        .unwrap_or_else(|_| panic!("Unable to open file: {}", input));

    let file = BufReader::new(file);

    let parser = EventReader::new(file);
    let mut depth = 0;
    for e in parser {
        match e {
            Ok(XmlEvent::StartElement { name, attributes,.. }) => {
                let attr: String = attributes
                    .iter()
                    .map(|attr: &xml::attribute::OwnedAttribute| format!("{}", attr.value))
                    .collect::<Vec<String>>()
                    .join(" ");
                println!("{:spaces$}+{name} - {attr}", "", spaces = depth * 2);
                depth += 1;
            }
            Ok(XmlEvent::EndElement { name }) => {
                depth -= 1;
                println!("\t{:spaces$}-{name}", "", spaces = depth * 2);
            }
            Err(e) => {
                eprintln!("Error: {e}");
                break;
            }
            // There's more: https://docs.rs/xml-rs/latest/xml/reader/enum.XmlEvent.html
            _ => {}
        }
    }

    // No need for explicit unit return
}
