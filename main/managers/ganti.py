with open("display_system.c", "r") as f:
    content = f.read()
content = content.replace("📶", "[W]").replace("🔒", "[L]").replace("⚡", "[!]").replace("👥", "[G]").replace("📊", "[#]").replace("📡", "[T]").replace("ℹ️", "[i]").replace("⛔", "[X]").replace("📺", "[TV]").replace("❄️", "[AC]").replace("💡", "[L]").replace("📄", "[F]").replace("📁", "[D]").replace("🌎", "[NA]").replace("🌍", "[EU]").replace("🌏", "[AL]").replace("✓", "[v]").replace("⋯", "...")
with open("display_system.c", "w") as f:
    f.write(content)