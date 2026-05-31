#include <TFile.h>
#include <TCanvas.h>
#include <TH2F.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Post-alignment correlation 2D check for all 3 MALTA planes
// Usage: check_correlation2D("output/allaligncheck_runXXX.root",
//                            "output/allaligncheck/correlation2D_runXXX.png")
// Layout: 2 rows (X, Y) x 3 cols (MALTA_0, MALTA_1, MALTA_2) = 6 pads
// ---------------------------------------------------------------------------
void check_correlation2D(TString filename, TString output_png) {

    TString run_str = filename;
    run_str.Remove(0, run_str.Last('/') + 1);
    run_str.ReplaceAll("allaligncheck_run", "");
    run_str.ReplaceAll(".root", "");

    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(99);

    TFile* f = TFile::Open(filename);
    if (!f || f->IsZombie()) {
        std::cout << "[CORR2D][ERROR] Cannot open: " << filename << std::endl;
        gSystem->Exit(1);
        return;
    }

    std::vector<TString> detectors = {"MALTA_0", "MALTA_1", "MALTA_2"};

    TCanvas* c = new TCanvas("corr2d", Form("Correlation 2D  Run %s", run_str.Data()), 1800, 900);

    TPad* p_title = new TPad("p_title", "", 0.0, 0.93, 1.0, 1.0);
    p_title->SetFillColor(kGray + 3);
    p_title->SetBorderMode(0);
    p_title->Draw();
    p_title->cd();
    TLatex title_tex;
    title_tex.SetNDC(); title_tex.SetTextFont(62);
    title_tex.SetTextColor(kWhite); title_tex.SetTextSize(0.50);
    title_tex.DrawLatex(0.02, 0.20,
        Form("MALTA2 Post-Alignment Correlation 2D   Run %s", run_str.Data()));
    c->cd();

    // 6 pads: row 0 = CorrX, row 1 = CorrY
    TPad* pads[2][3];
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 3; col++) {
            double xlo = col / 3.0;
            double xhi = (col + 1) / 3.0;
            double ylo = (1 - row) * 0.465;
            double yhi = (1 - row) * 0.465 + 0.465;
            pads[row][col] = new TPad(Form("pad_%d_%d", row, col), "", xlo, ylo, xhi, yhi);
            pads[row][col]->SetRightMargin(0.15);
            pads[row][col]->SetLeftMargin(0.12);
            pads[row][col]->SetTopMargin(0.13);
            pads[row][col]->SetBottomMargin(0.12);
            pads[row][col]->Draw();
        }
    }

    const char* axes[2] = {"X", "Y"};
    bool any_missing = false;

    for (int col = 0; col < 3; col++) {
        TString det = detectors[col];
        for (int row = 0; row < 2; row++) {
            pads[row][col]->cd();
            TString path = Form("Correlations/%s/correlation%s_2Dlocal", det.Data(), axes[row]);
            TH2F* h = (TH2F*)f->Get(path);

            if (!h) {
                TLatex miss; miss.SetNDC(); miss.SetTextSize(0.07); miss.SetTextColor(kRed);
                miss.DrawLatex(0.1, 0.5, "not found");
                std::cout << "[CORR2D][WARN] Missing: " << path << std::endl;
                any_missing = true;
                continue;
            }

            h->SetDirectory(0);
            h->GetXaxis()->SetTitle(Form("Reference %s [px]", axes[row]));
            h->GetYaxis()->SetTitle(Form("%s %s [px]", det.Data(), axes[row]));
            h->GetXaxis()->SetTitleSize(0.055); h->GetXaxis()->SetLabelSize(0.048);
            h->GetYaxis()->SetTitleSize(0.055); h->GetYaxis()->SetLabelSize(0.048);
            h->Draw("colz");

            TLatex lat; lat.SetNDC();
            lat.SetTextFont(62); lat.SetTextSize(0.065); lat.SetTextColor(kBlack);
            lat.DrawLatex(0.15, 0.88, det.Data());
            lat.SetTextFont(42); lat.SetTextSize(0.060);
            lat.DrawLatex(0.15, 0.80, Form("Corr%s 2D", axes[row]));
        }
    }

    c->SaveAs(output_png);
    std::cout << "[CORR2D] Saved: " << output_png << std::endl;

    f->Close();
    if (any_missing) gSystem->Exit(1);
}
