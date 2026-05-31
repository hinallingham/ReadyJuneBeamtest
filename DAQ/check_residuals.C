#include <TFile.h>
#include <TCanvas.h>
#include <TH1F.h>
#include <TF1.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Post-alignment residual distribution check for all 3 MALTA planes
// Usage: check_residuals("output/allaligncheck_runXXX.root",
//                        "output/allaligncheck/residuals_runXXX.png")
// Layout: 2 rows (ResX, ResY) x 3 cols (MALTA_0, MALTA_1, MALTA_2) = 6 pads
// Gaussian fit with mean and sigma annotated on each pad
// ---------------------------------------------------------------------------
void check_residuals(TString filename, TString output_png) {

    TString run_str = filename;
    run_str.Remove(0, run_str.Last('/') + 1);
    run_str.ReplaceAll("allaligncheck_run", "");
    run_str.ReplaceAll(".root", "");

    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);

    TFile* f = TFile::Open(filename);
    if (!f || f->IsZombie()) {
        std::cout << "[RESIDUALS][ERROR] Cannot open: " << filename << std::endl;
        gSystem->Exit(1);
        return;
    }

    std::vector<TString> detectors = {"MALTA_0", "MALTA_1", "MALTA_2"};

    TCanvas* c = new TCanvas("residuals", Form("Residuals  Run %s", run_str.Data()), 1800, 900);

    TPad* p_title = new TPad("p_title", "", 0.0, 0.93, 1.0, 1.0);
    p_title->SetFillColor(kGray + 3);
    p_title->SetBorderMode(0);
    p_title->Draw();
    p_title->cd();
    TLatex title_tex;
    title_tex.SetNDC(); title_tex.SetTextFont(62);
    title_tex.SetTextColor(kWhite); title_tex.SetTextSize(0.50);
    title_tex.DrawLatex(0.02, 0.20,
        Form("MALTA2 Post-Alignment Residuals   Run %s", run_str.Data()));
    c->cd();

    // 6 pads: row 0 = ResX, row 1 = ResY
    TPad* pads[2][3];
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 3; col++) {
            double xlo = col / 3.0;
            double xhi = (col + 1) / 3.0;
            double ylo = (1 - row) * 0.465;
            double yhi = (1 - row) * 0.465 + 0.465;
            pads[row][col] = new TPad(Form("pad_%d_%d", row, col), "", xlo, ylo, xhi, yhi);
            pads[row][col]->SetRightMargin(0.05);
            pads[row][col]->SetLeftMargin(0.14);
            pads[row][col]->SetTopMargin(0.13);
            pads[row][col]->SetBottomMargin(0.13);
            pads[row][col]->Draw();
        }
    }

    const char* axes[2]  = {"X", "Y"};
    const char* res_names[2] = {"LocalResidualsX", "LocalResidualsY"};
    bool any_missing = false;

    for (int col = 0; col < 3; col++) {
        TString det = detectors[col];
        for (int row = 0; row < 2; row++) {
            pads[row][col]->cd();
            TString path = Form("Tracking4D/%s/local_residuals/%s", det.Data(), res_names[row]);
            TH1F* h = (TH1F*)f->Get(path);

            if (!h) {
                TLatex miss; miss.SetNDC(); miss.SetTextSize(0.07); miss.SetTextColor(kRed);
                miss.DrawLatex(0.1, 0.5, "not found");
                std::cout << "[RESIDUALS][WARN] Missing: " << path << std::endl;
                any_missing = true;
                continue;
            }

            h->SetDirectory(0);
            h->SetLineColor(kBlue + 2);
            h->SetLineWidth(2);
            h->GetXaxis()->SetTitle(Form("Residual %s [mm]", axes[row]));
            h->GetYaxis()->SetTitle("Entries");
            h->GetXaxis()->SetTitleSize(0.055); h->GetXaxis()->SetLabelSize(0.048);
            h->GetYaxis()->SetTitleSize(0.055); h->GetYaxis()->SetLabelSize(0.048);
            h->Draw();

            // Gaussian fit
            h->Fit("gaus", "Q");
            TF1* fit = h->GetFunction("gaus");
            if (fit) {
                fit->SetLineColor(kRed);
                fit->SetLineWidth(2);
                fit->Draw("same");

                double mean  = fit->GetParameter(1);
                double sigma = fit->GetParameter(2);

                TLatex lat; lat.SetNDC();
                lat.SetTextFont(62); lat.SetTextSize(0.063); lat.SetTextColor(kBlack);
                lat.DrawLatex(0.55, 0.88, det.Data());
                lat.SetTextFont(42); lat.SetTextColor(kRed); lat.SetTextSize(0.058);
                lat.DrawLatex(0.55, 0.79, Form("Res%s", axes[row]));
                lat.SetTextFont(62);
                lat.DrawLatex(0.55, 0.70, Form("#mu = %.1f #mum", mean  * 1000.0));
                lat.DrawLatex(0.55, 0.61, Form("#sigma = %.1f #mum", sigma * 1000.0));
            }
        }
    }

    c->SaveAs(output_png);
    std::cout << "[RESIDUALS] Saved: " << output_png << std::endl;

    f->Close();
    if (any_missing) gSystem->Exit(1);
}
