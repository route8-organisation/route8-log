use tokio::io::AsyncReadExt;

pub mod debug;
pub mod config;
pub mod log_queue;
pub mod log_journald;

#[tokio::main]
async fn main() {
    if let Err(e) = debug::initialize() {
        eprintln!("failed to initialize the debug module, error: {}", e.to_string());
        return;
    }

    outputln!("app", "############## STARTED ##############");

    if let Err(e) = config::initialize().await {
        errorln!("app", "failed to load the config, error: {}", e.to_string());
        return;
    }

    if let Err(e) = log_queue::initialize().await {
        errorln!("app", "failed to initialize the log queue, error: {}", e.to_string());
        return;
    }

    log_journald::initialize().await;

    if let Err(e) = log_journald::start("hello-world".to_owned()).await {
        panic!("TEST FAILED: {}", e.to_string());
    }

    loop {
        let _ = tokio::io::stdin().read_u8().await;
        println!("EXITING");
        std::process::exit(0);
    }
}
