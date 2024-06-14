use anyhow::anyhow;
use once_cell::sync::Lazy;

use crate::outputln;

#[derive(Debug, Clone, serde::Deserialize)]
pub struct ConfigFile {
    pub verbose: bool,
    pub dispatch_sleep_ms: u64,
    pub maximum_log_entries: usize,
    pub remote_certificate: String,
    pub remote_address: String,
    pub remote_port: u16,
    pub network_mtu: usize,
    pub secure_identity: String,
    pub secure_identity_password: String,
}

static CONFIG_FILE_LOADED: Lazy<tokio::sync::Mutex<Option<ConfigFile>>> = Lazy::new(|| 
    tokio::sync::Mutex::new(None)
);

const CONFIG_FILENAME: &str = "config.yml";

pub async fn get_clone() -> ConfigFile {
    CONFIG_FILE_LOADED.lock().await.clone().expect("config file is not loaded")
}

pub async fn initialize() -> anyhow::Result<()> {
    let config_filename = std::path::Path::new(CONFIG_FILENAME);

    if !config_filename.exists() {
        return Err(anyhow!(format!("missing file '{}'", CONFIG_FILENAME)));
    }

    let file_content = std::fs::read_to_string(config_filename).map_err(|e| anyhow!("failed to load '{}' due to {}", CONFIG_FILENAME, e.to_string()))?;
    let object: ConfigFile = serde_yaml::from_str(&file_content).map_err(|e| anyhow!("failed to deserialize the config file '{}' due to {}", CONFIG_FILENAME, e.to_string()))?;

    outputln!("config", "config '{}' loaded successfuly", CONFIG_FILENAME);
    outputln!("config", "# verbose = '{}'", object.verbose);
    outputln!("config", "# dispatch_sleep_ms = '{}'", object.dispatch_sleep_ms);
    outputln!("config", "# maximum_log_entries = '{}'", object.maximum_log_entries);
    outputln!("config", "# remote_certificate = '{}'", object.remote_certificate);
    outputln!("config", "# remote_address = '{}'", object.remote_address);
    outputln!("config", "# remote_port = '{}'", object.remote_port);
    outputln!("config", "# network_mtu = '{}'", object.network_mtu);
    outputln!("config", "# secure_identity = '{}'", object.secure_identity);
    outputln!("config", "# secure_identity_password = '{}'", object.secure_identity_password);
    
    let mut acquired_global_memory = CONFIG_FILE_LOADED.lock().await;
    *acquired_global_memory = Some(object);

    Ok(())
}