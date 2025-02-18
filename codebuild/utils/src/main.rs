use aws_config::meta::region::RegionProviderChain;
use aws_config::BehaviorVersion;
use aws_sdk_codebuild::{Client, Error};
use tokio::{io::AsyncWriteExt, net::TcpStream};

#[tokio::main]
async fn main() -> Result<(), Error> {
    let region_provider = RegionProviderChain::default_provider().or_else("us-west-2");
    let config = aws_config::defaults(BehaviorVersion::latest())
        .region(region_provider)
        .load()
        .await;
    let client = Client::new(&config);

    let resp = client.batch_get_projects().set_names("s2nGeneralBatch").send().await?;
    println!("Buildspecs:");

    let names = resp.projects();

    for name in names {
        println!("  {:?}", name.source());
    }

    println!();
    println!("Found {} tables", names.len());

    Ok(())
}
