#include <TFile.h>
#include <TCanvas.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TStyle.h>
#include <TString.h>
#include <TLatex.h>
#include <TROOT.h>
#include <iostream>
#include <vector>

// -------------------------------------------------------------------------
// Macro to plot 2D Correlations and 1D Residuals for 3 MALTA2 planes
// -------------------------------------------------------------------------
void check_full_qc(TString filename) {
    // Disable default stat box for a cleaner look
    gStyle->SetOptStat(0);
    gStyle->SetOptFit(0);

    // Open the target ROOT file
    TFile *file = TFile::Open(filename);
    if (!file || file->IsZombie()) {
        std::cout << "[ERROR] Cannot open file: " << filename << std::endl;
        return;
    }

    std::cout << "[INFO] Loading Full Quality Control Dashboard from: " << filename << std::endl;

    // Create a massive canvas: 4 columns (CorrX, CorrY, ResX, ResY) x 3 rows (Planes)
    // Dynamic allocation ensures the canvas survives outside the function execution scope
    TCanvas *c1 = new TCanvas("c1", "Telescope Full Alignment QC", 1800, 1000);
    c1->Divide(4, 3);

    std::vector<TString> detectors = {"MALTA_0", "MALTA_1", "MALTA_2"};
    int padIndex = 1;
    
    for (size_t i = 0; i < detectors.size(); ++i) {
        TString det = detectors[i];

        // Define paths for the 4 plots per plane
        TString pathCorrX = Form("Correlations/%s/correlationX_2Dlocal", det.Data());
        TString pathCorrY = Form("Correlations/%s/correlationY_2Dlocal", det.Data());
        TString pathResX  = Form("Tracking4D/%s/local_residuals/LocalResidualsX", det.Data());
        TString pathResY  = Form("Tracking4D/%s/local_residuals/LocalResidualsY", det.Data());

        // --- 1. Draw Correlation X (2D) ---
        c1->cd(padIndex++);
        TH2F *hCorrX = (TH2F*)file->Get(pathCorrX);
        if (hCorrX) {
            hCorrX->SetTitle(Form("%s 2D Correlation X", det.Data()));
            // Detach histogram from file directory to keep it in memory
            hCorrX->SetDirectory(0); 
            hCorrX->Draw("colz");
        } else {
            std::cout << "[WARN] Missing: " << pathCorrX << std::endl;
        }

        // --- 2. Draw Correlation Y (2D) ---
        c1->cd(padIndex++);
        TH2F *hCorrY = (TH2F*)file->Get(pathCorrY);
        if (hCorrY) {
            hCorrY->SetTitle(Form("%s 2D Correlation Y", det.Data()));
            hCorrY->SetDirectory(0);
            hCorrY->Draw("colz");
        } else {
            std::cout << "[WARN] Missing: " << pathCorrY << std::endl;
        }

        // Helper lambda for Residuals (1D with Fit)
        auto drawResidual = [&](TString path, TString title) {
            int currentPad = padIndex++;
            c1->cd(currentPad);
            TH1F *hRes = (TH1F*)file->Get(path);
            if (hRes) {
                hRes->SetTitle(title);
                hRes->SetDirectory(0); // Safely detach from file registry
                hRes->SetLineColor(kBlue + 2);
                //hRes->SetFillColor(kAzure + 7);
                hRes->Draw();

                hRes->Fit("gaus", "Q");
                TF1 *fit = hRes->GetFunction("gaus");

                if (fit) {
                    double mean = fit->GetParameter(1);
                    double sigma = fit->GetParameter(2);

                    // Draw Bold Red Text
                    TLatex latex;
                    latex.SetNDC();
                    latex.SetTextFont(62);    // 62 = Helvetica Bold
                    latex.SetTextColor(kRed); // Red text
                    latex.SetTextSize(0.06);

                    latex.DrawLatex(0.55, 0.80, Form("Mean: %.5f", mean));
                    latex.DrawLatex(0.55, 0.72, Form("#sigma: %.5f", sigma));
                }
            } else {
                std::cout << "[WARN] Missing: " << path << std::endl;
            }
        };

        // --- 3 & 4. Draw Residual X and Y ---
        drawResidual(pathResX, Form("%s Residual X", det.Data()));
        drawResidual(pathResY, Form("%s Residual Y", det.Data()));
    }

    // Force rendering all subpads into graphics card pipeline
    c1->Modified();
    c1->Update();
    
    // Export static outputs for offline/Discord monitors
    c1->SaveAs("alignment_qc_result.png");
    c1->SaveAs("alignment_qc_result.pdf");
    
    // Crucial: Do NOT call file->Close() here, let it remain open in the session 
    // so that the graphics server can safely fetch data structures interactively.
    std::cout << "[DONE] Full QC Dashboard is completely rendered." << std::endl;
}