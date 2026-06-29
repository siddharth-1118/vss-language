class Vss < Formula
  desc "The VSS Programming Language Compiler and VM"
  homepage "https://github.com/siddharth-1118/vss-language"
  url "https://github.com/siddharth-1118/vss-language/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "a43878b27376c66d11b347e33555577ee66666bb66a4a622a6a6a43878b27376" # Replace with actual release tarball sha256
  license "MIT"

  depends_on "make" => :build
  depends_on "gcc" => :build

  def install
    # Enter build directory and compile
    cd "vss" do
      system "make", "clean"
      system "make"
      bin.install "vss"
    end
  end

  test do
    # Simple CLI self-test
    assert_match "VSS Version: v1.0.0", shell_output("#{bin}/vss --version")
  end
end
