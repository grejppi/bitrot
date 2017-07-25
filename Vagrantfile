Vagrant.configure("2") do |config|
  config.vm.box = "bento/ubuntu-16.04"

  config.vm.provision "shell", inline: <<-EOF
    apt-get install -y g++ g++-multilib mingw-w64 mingw-w64-x86-64-dev mingw-w64-i686-dev g++-mingw-w64-i686 g++-mingw-w64-x86-64
  EOF
end
