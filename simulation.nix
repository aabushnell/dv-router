{ pkgs, lib, ... }:

let
  routerPackage = pkgs.stdenv.mkDerivation {
    name = "dv-router";
    src = /home/aaron/Documents/dev/dv-router;
    buildInputs = [ pkgs.gcc ];
    buildPhase = "make all";
  };
in
{
  # HOST BRIDGES
  networking.bridges = {
    linkHost1 = { interfaces = []; }; # Cable: Host1 <-> RouterA
    linkAB    = { interfaces = []; }; # Cable: RouterA <-> RouterB
    linkBC    = { interfaces = []; }; # Cable: RouterB <-> RouterC
    linkHost2 = { interfaces = []; }; # Cable: RouterC <-> Host2
  };

  environment.etc."NetworkManager/conf.d/99-simulation-unmanaged.conf".text = ''
    [keyfile]
    unmanaged-devices=interface-name:link*;interface-name:eth*;interface-name:vb-*
  '';

  systemd.services.NetworkManager-wait-online.enable = false;

  networking.interfaces.linkHost1.ipv4.addresses = [
    { address = "10.99.1.1"; prefixLength = 24; }
  ];
  networking.interfaces.linkAB.ipv4.addresses = [
    { address = "10.99.2.1"; prefixLength = 24; }
  ];
  networking.interfaces.linkBC.ipv4.addresses = [
    { address = "10.99.3.1"; prefixLength = 24; }
  ];
  networking.interfaces.linkHost2.ipv4.addresses = [
    { address = "10.99.4.1"; prefixLength = 24; }
  ];

  systemd.services.create-sim-bridges = {
    description = "Manually Create Simulation Bridges";
    before = [ 
      "network.target"
      "container@routerA.service"
      "container@routerB.service"
      "container@routerC.service"
    ];
    wantedBy = [ "multi-user.target" ];
    serviceConfig = {
      Type = "oneshot";
      RemainAfterExit = true;
    };
    script = ''
      ${pkgs.iproute2}/bin/ip link add name linkHost1 type bridge || true
      ${pkgs.iproute2}/bin/ip link add name linkAB    type bridge || true
      ${pkgs.iproute2}/bin/ip link add name linkBC    type bridge || true
      ${pkgs.iproute2}/bin/ip link add name linkHost2 type bridge || true

      ${pkgs.iproute2}/bin/ip link set linkHost1 up
      ${pkgs.iproute2}/bin/ip link set linkAB up
      ${pkgs.iproute2}/bin/ip link set linkBC up
      ${pkgs.iproute2}/bin/ip link set linkHost2 up
    '';
  };

  # ROUTER CONTAINERS
  containers.routerA = {
    autoStart = false;
    privateNetwork = true;
    extraVeths = {
      ethA0 = { hostBridge = "linkHost1"; }; # Connects to Host1
      ethA1 = { hostBridge = "linkAB"; };    # Connects to RouterB
    };
    config = { pkgs, ... }: {
      system.stateVersion = "25.05";

      environment.systemPackages = [
        pkgs.socat
        pkgs.tcpdump
      ];

      networking.useNetworkd = true;
      networking.useDHCP = false;
      networking.useHostResolvConf = false;
      boot.kernel.sysctl = {
        "net.ipv4.ip_forward" = 1;
        "net.ipv4.conf.all.forwarding" = 1;
      };

      # Interfaces
      networking.interfaces.ethA0.ipv4.addresses = [
        { address = "192.168.1.1"; prefixLength = 24; }
      ];
      networking.interfaces.ethA1.ipv4.addresses = [
        { address = "10.1.0.10"; prefixLength = 24; }
      ];

      networking.firewall = {
        allowedUDPPorts = [ 5555 ];
        checkReversePath = false;
      };

      systemd.services.dv-router = {
        description = "Distance Vector Router Daemon";
        wantedBy = [ "multi-user.target" ];
        serviceConfig = {
          # ExecStart = "${routerPackage}/bin/dv-router";
          ExecStart = "${pkgs.coreutils}/bin/sleep infinity";
          Restart = "always";
        };
      };
    };
  };

  containers.routerB = {
    autoStart = false;
    privateNetwork = true;
    extraVeths = {
      ethB1 = { hostBridge = "linkAB"; }; # Connects to RouterA
      ethB2 = { hostBridge = "linkBC"; }; # Connects to RouterC
    };
    config = { pkgs, ... }: {
      system.stateVersion = "25.05";

      environment.systemPackages = [
        pkgs.socat
        pkgs.tcpdump
      ];

      networking.useNetworkd = true;
      networking.useDHCP = false;
      networking.useHostResolvConf = false;
      boot.kernel.sysctl = {
        "net.ipv4.ip_forward" = 1;
        "net.ipv4.conf.all.forwarding" = 1;
      };

      # Interfaces
      networking.interfaces.ethB1.ipv4.addresses = [
        { address = "10.1.0.11"; prefixLength = 24; }
      ];
      networking.interfaces.ethB2.ipv4.addresses = [
        { address = "10.2.0.11"; prefixLength = 24; }
      ];

      networking.firewall = {
        allowedUDPPorts = [ 5555 ];
        checkReversePath = false;
      };

      systemd.services.dv-router = {
        description = "Distance Vector Router Daemon";
        wantedBy = [ "multi-user.target" ];
        serviceConfig = {
          # ExecStart = "${routerPackage}/bin/dv-router";
          ExecStart = "${pkgs.coreutils}/bin/sleep infinity";
          Restart = "always";
        };
      };
    };
  };

  containers.routerC = {
    autoStart = false;
    privateNetwork = true;
    extraVeths = {
      ethC2 = { hostBridge = "linkBC"; };    # Connects to RouterB
      ethC3 = { hostBridge = "linkHost2"; }; # Connects to Host2
    };
    config = { pkgs, ... }: {
      system.stateVersion = "25.05";

      environment.systemPackages = [
        pkgs.socat
        pkgs.tcpdump
      ];

      networking.useNetworkd = true;
      networking.useDHCP = false;
      networking.useHostResolvConf = false;
      boot.kernel.sysctl = {
        "net.ipv4.ip_forward" = 1;
        "net.ipv4.conf.all.forwarding" = 1;
      };

      # Interfaces
      networking.interfaces.ethC2.ipv4.addresses = [
        { address = "10.2.0.12"; prefixLength = 24; }
      ];
      networking.interfaces.ethC3.ipv4.addresses = [
        { address = "192.168.2.1"; prefixLength = 24; }
      ];

      networking.firewall = {
        allowedUDPPorts = [ 5555 ];
        checkReversePath = false;
      };

      systemd.services.dv-router = {
        description = "Distance Vector Router Daemon";
        wantedBy = [ "multi-user.target" ];
        serviceConfig = {
          # ExecStart = "${routerPackage}/bin/dv-router";
          ExecStart = "${pkgs.coreutils}/bin/sleep infinity";
          Restart = "always";
        };
      };
    };
  };

  # HOST CONTAINERS
  containers.host1 = {
    autoStart = false;
    privateNetwork = true;
    extraVeths = {
      ethH1 = { hostBridge = "linkHost1"; }; # Connects to RouterA
    };
    config = { pkgs, ... }: {
      system.stateVersion = "25.05";

      environment.systemPackages = [
        pkgs.socat
        pkgs.tcpdump
      ];

      networking.useNetworkd = true;
      networking.useDHCP = false;
      networking.useHostResolvConf = false;

      # Interfaces
      networking.interfaces.ethH1.ipv4.addresses = [
        { address = "192.168.1.10"; prefixLength = 24; }
      ];

      # Default Gateway
      networking.defaultGateway = {
        address = "192.168.1.1";
        interface = "ethH1";
      };
    };
  };

  containers.host2 = {
    autoStart = false;
    privateNetwork = true;
    extraVeths = {
      ethH2 = { hostBridge = "linkHost2"; }; # Connects to RouterC
    };
    config = { pkgs, ... }: {
      system.stateVersion = "25.05";

      environment.systemPackages = [
        pkgs.socat
        pkgs.tcpdump
      ];

      networking.useNetworkd = true;
      networking.useDHCP = false;
      networking.useHostResolvConf = false;

      # Interfaces
      networking.interfaces.ethH2.ipv4.addresses = [
        { address = "192.168.2.10"; prefixLength = 24; }
      ];

      # Default Gateway
      networking.defaultGateway = {
        address = "192.168.2.1";
        interface = "ethH2";
      };
    };
  };
}
